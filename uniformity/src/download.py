'''
    download.py - A program that downloads models
    Copyright (C) 2025 PlayfulMathematician

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

'''
import json
import os
import warnings
warnings.filterwarnings("ignore", category=DeprecationWarning) # shut it bs4
from bs4 import BeautifulSoup
import requests
headers = {"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/137.0.0.0 Safari/537.36"}

def e(url, depth=3):
    if depth < 0:
        print("I give up")
        os._exit(0)
    elif depth < 3:
        print(f"We failed {3-depth} times")
        print("Retrying")
    else:
        pass
    # miraheze gets angry when useragent is bad
    j = requests.get(url, headers=headers)
    if j.status_code != 200:
        return e(url,depth-1)
    soup = BeautifulSoup(j.text, features="html.parser")
    author=soup.find("td", id="fileinfotpl_aut").find_next_sibling("td").get_text(strip=True)
    file=soup.find("div", id="file").findChildren()[0].get("href")
    return [file, author]
    

def main(): 
    try:
        os.mkdir("../media/models")
    except Exception as qa:
        pass
    with open("../media/models/modellist.json", "r") as f:
        out = json.load(f)
    contributors = []
    for i in out:
        z = e(i[0])
        if z[1] not in contributors:
            contributors.append(z[1])

        with open("../media/models/" + i[1] + ".off", "w") as q:
            j = requests.get("https:"+z[0], headers=headers)
            j.raise_for_status()
            q.write(j.text)
    contrib_path = "../media/models/CONTRIBUTIONS.md"
    with open(contrib_path, "w", encoding="utf-8") as f:
        f.write("# Contributors\n\n")
        for name in sorted(contributors):
            f.write(f"- {name}\n")
    

        

if __name__ == "__main__":
    main()
