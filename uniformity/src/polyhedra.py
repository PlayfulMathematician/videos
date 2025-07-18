'''
polyhedra.py - A program that includes many classes relating to polyhedra rendering in manim
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

from typing import Optional
from math import sqrt
from .exceptions import NormalizationError


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
            raise NormalizationError
        return self / self.magnitude
    
    def __rmul__(self, scalar: float) -> "pVector":
        return self * scalar
    
    def __eq__(self, other: "pVector") -> bool:
        return (self - other).magnitude == 0
    
    def __setattr__(self, *_, **__):
        raise AttributeError("no i don't want to")
