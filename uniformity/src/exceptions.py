'''
exceptions.py - This program includes all the exceptions I will throw.
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

class NormalizationError(ZeroDivisionError):
    def __init__(self, message = "You cannot normalize the zero vector."):
        self.message = message
        super().__init__(self.message)
