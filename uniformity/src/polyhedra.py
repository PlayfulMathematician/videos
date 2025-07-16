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

import manim
from typing import Optional
from math import sqrt
from exceptions import NormalizationError


class P_Vector:
    def __init__(
        self, 
        x: Optional[float] = None, 
        y: Optional[float] = None, 
        z: Optional[float] = None
    ) -> None:
        self.x = x if x is not None else 0
        self.y = y if y is not None else 0
        self.z = z if z is not None else 0

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
        return f"P_Vector({self.x}, {self.y}, {self.z})"

    def __str__(self) -> str:
        return f"({self.x}, {self.y}, {self.z})"
    
    @property
    def magnitude(self) -> float:
        return sqrt(self.x ** 2 + self.y ** 2 + self.z ** 2)
    
    def __mul__(self, scalar: float) -> "P_Vector":
        return P_Vector(self.x * scalar, self.y * scalar, self.z * scalar)

    def __add__(self, vector: "P_Vector") -> "P_Vector":
        return P_Vector(self.x + vector.x, self.y + vector.y, self.z + vector.z)
    
    def __neg__(self) -> "P_Vector":
        return P_Vector(-self.x, -self.y, -self.z)

    def __sub__(self, vector: "P_Vector"):
        return self + (-vector)


    


