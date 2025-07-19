'''
main.py - A program that does all rendering and class initialization
    Copyright (C) 2025 PlayfulMathematician

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
'''
# ────────────────────────────────
# IMPORTS
# ────────────────────────────────

from typing import Optional
from math import sqrt
# ────────────────────────────────
# METADATA
# ────────────────────────────────

__version__ = "0.1.0"
__author__ = "PlayfulMathematician"
__license__ = "GPL-3.0"
__all__ = [
    "pVector",
    "NormalizationError"
]
__email__ = "me@playfulmathematician.com"

# ────────────────────────────────
# CONSTANTS
# ────────────────────────────────



# ────────────────────────────────
# EXCEPTIONS
# ────────────────────────────────

class NormalizationError(ZeroDivisionError):
    def __init__(self, message = "You cannot normalize the zero vector."):
        self.message = message
        super().__init__(self.message)

# ────────────────────────────────
# CLASSES
# ────────────────────────────────

class pVector:
    __slots__ = ('_x', '_y', '_z')
    def __init__(
        self, 
        x: Optional[float] = None, 
        y: Optional[float] = None, 
        z: Optional[float] = None
    ) -> None:
        object.__setattr__(self, '_x', x if x is not None else 0)
        object.__setattr__(self, '_y', y if y is not None else 0)
        object.__setattr__(self, '_z', z if z is not None else 0)
        
    @property
    def x(self):
        return self._x
    
    @property
    def y(self):
        return self._y
    
    @property
    def z(self):
        return self._z
    
    def __getitem__(self, index: int) -> float:
        match index:
            case 0:
                return self.x
            case 1:
                return self.y
            case 2:
                return self.z
            case _:
                raise IndexError("point index out of range")

    def __repr__(self) -> str:
        return f"pVector({self.x}, {self.y}, {self.z})"

    def __str__(self) -> str:
        return f"({self.x}, {self.y}, {self.z})"
    
    @property
    def magnitude(self) -> float:
        return sqrt(self.x ** 2 + self.y ** 2 + self.z ** 2)
    
    def __mul__(self, scalar: float) -> "pVector":
        return pVector(self.x * scalar, self.y * scalar, self.z * scalar)

    def __add__(self, vector: "pVector") -> "pVector":
        return pVector(self.x + vector.x, self.y + vector.y, self.z + vector.z)
    
    def __neg__(self) -> "pVector":
        return pVector(-self.x, -self.y, -self.z)

    def __sub__(self, vector: "pVector") -> "pVector":
        return self + (-vector)
    
    def __truediv__(self, scalar: float) -> "pVector":
        if scalar == 0:
            raise ZeroDivisionError("You cannot divide a vector by zero")
        return self * (1 / scalar)

    def normalize(self) -> "pVector":
        if self.magnitude == 0:
            raise NormalizationError()
        return self / self.magnitude
    
    def __rmul__(self, scalar: float) -> "pVector":
        return self * scalar
    
    def dist(self, other: "pVector") -> float:
        return (self - other).magnitude
    
    def __eq__(self, other: "pVector") -> bool:
        return self.dist(other) == 0
    
    def __setattr__(self, *_, **__):
        raise AttributeError("no i don't want to")

    def dot_product(self, other: "pVector") -> float:
        return self.x * other.x + self.y * other.y + self.z * other.z

    def cross_product(self, other: "pVector") -> "pVector":
        return pVector(self.y * other.z - self.z * other.y, self.z * other.x - self.x * other.z, self.x * other.y - self.y * other.x)
    
    def __len__(self) -> int:
        return 3
    
    def __iter__(self):
        yield self.x
        yield self.y
        yield self.z
    
    def __hash__(self) -> int:
        return hash((self.x, self.y, self.z))
    
class pLine:
    __slots__ = ('_point', '_direction')
    
    def __init__(self, point: pVector, direction: pVector) -> None:
        object.__setattr__(self, '_point', point)
        object.__setattr__(self, '_direction', direction.normalize())
    
    @property
    def point(self) -> pVector:
        return self._point
    
    @property
    def direction(self) -> pVector:
        return self._direction
    
    def __repr__(self) -> str:
        return f"pLine(point={self.point}, direction={self.direction})"
    
class pLineSegment(pLine):
    __slots__ = ('_start', '_end')
    
    def __init__(self, start: pVector, end: pVector) -> None:
        super().__init__(start, end - start)
        object.__setattr__(self, '_start', start)
        object.__setattr__(self, '_end', end)
    
    @property
    def start(self) -> pVector:
        return self._start
    
    @property
    def end(self) -> pVector:
        return self._end
    
    def __repr__(self) -> str:
        return f"pLineSegment(start={self.start}, end={self.end})"

