"""Optional asynchronous motion predictor for Orbital Eye.

The UDP protocol deliberately carries only the newest observation. When a
TorchScript model is supplied, it predicts the latency-compensated position;
otherwise the constant-velocity estimator keeps the C loop fully usable.
"""

from __future__ import annotations

import argparse
import importlib
import importlib.util
import json
import socket
from collections import deque
from pathlib import Path
from typing import Any

torch: Any = None
if importlib.util.find_spec("torch") is not None:
    torch = importlib.import_module("torch")


class MotionPredictor:
    def __init__(self, model_path: Path | None = None) -> None:
        self.history: deque[tuple[float, float, float]] = deque(maxlen=8)
        self.model: Any = None
        if torch is not None and model_path is not None:
            self.model = torch.jit.load(str(model_path), map_location="cpu")
            self.model.eval()

    def predict(self, x: float, y: float, delta: float, latency: float) -> tuple[float, float, float]:
        delta = max(delta, 1e-4)
        self.history.append((x, y, delta))
        if self.model is not None and len(self.history) >= 4:
            values = []
            for px, py, dt in self.history:
                values.extend((px / 640.0, py / 480.0, min(dt, 0.25)))
            values.extend((min(max(latency, 0.0), 0.25),))
            with torch.no_grad():
                output = self.model(torch.tensor([values], dtype=torch.float32))[0]
            return float(output[0] * 640.0), float(output[1] * 480.0), 0.9

        if len(self.history) < 2:
            return x, y, 0.0
        previous_x, previous_y, previous_delta = self.history[-2]
        velocity_x = (x - previous_x) / max(previous_delta, 1e-4)
        velocity_y = (y - previous_y) / max(previous_delta, 1e-4)
        horizon = min(max(latency, 0.0), 0.25)
        confidence = min(0.85, 0.45 + len(self.history) * 0.05)
        return x + velocity_x * horizon, y + velocity_y * horizon, confidence


def serve(port: int, model_path: Path | None) -> None:
    predictor = MotionPredictor(model_path)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
    sock.bind(("127.0.0.1", port))
    while True:
        payload, address = sock.recvfrom(1024)
        sock.setblocking(False)
        while True:
            try:
                payload, address = sock.recvfrom(1024)
            except BlockingIOError:
                break
        sock.setblocking(True)
        message = json.loads(payload)
        x, y, confidence = predictor.predict(
            float(message["x"]), float(message["y"]), float(message["dt"]), float(message["latency"])
        )
        response = json.dumps({"x": x, "y": y, "confidence": confidence}, separators=(",", ":"))
        sock.sendto(response.encode("ascii"), address)


def main() -> None:
    parser = argparse.ArgumentParser(description="Orbital Eye asynchronous motion predictor")
    parser.add_argument("--port", type=int, default=47001)
    parser.add_argument("--model", type=Path)
    args = parser.parse_args()
    serve(args.port, args.model)


if __name__ == "__main__":
    main()