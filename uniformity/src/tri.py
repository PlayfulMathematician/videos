c=0
for i in range(0, 4):
    for j in range(0,7):
        for k in range(7):
            for l in range(7):
                if (i == 3 and (j+k+l) != 0):
                    continue
                if j == k == l and j!=1:
                    continue
                c+=1;
                print(f"{i}{j}{k}{l}")

print(c)