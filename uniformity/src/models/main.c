#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
typedef struct {
    float x, y, z;
} Vec3;

Vec3 compute_normal(Vec3 v0, Vec3 v1, Vec3 v2)
{
    Vec3 u = {v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
    Vec3 v = {v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};
    Vec3 n = {
        u.y * v.z - u.z * v.y,
        u.z * v.x - u.x * v.z,
        u.x * v.y - u.y * v.x
    };
    float len = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
    if (len > 1e-8f)
    {
        n.x /= len; n.y /= len; n.z /= len;
    }
    else 
    {
        n.x = 0; n.y = 0; n.z = 0;
    }
    return n;
}

int read_off_header(FILE *fin, int *nv, int *nf)
{
    char line[256];
    if (!fgets(line, sizeof(line), fin))
    {
        return 0;
    }
    if (strncmp(line, "OFF", 3) != 0)
    {
        return 0;
    }
    do 
    {
        if (!fgets(line, sizeof(line), fin))
        {
            return 0;
        }
    }
    while (line[0] == '#' || line[0] == '\n');
    int _;
    int* __ = &_;
    
    if (sscanf(line, "%d %d %d", nv, nf, __) != 3)
    {
        return 0;
    }
    return 1;
}
int main(int argc, char *argv[])
{
    if (argc < 2) 
    {
        fprintf(stderr, "Usage: %s filename.off\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    FILE *fin = fopen(filename, "r");
    if (!fin) 
    {
        fprintf(stderr, "Error: Could not open file.\n");
        return 1;   
    }
    int nv, nf;
    if (!read_off_header(fin, &nv, &nf)) 
    {
        fprintf(stderr, "Error: Invalid or malformed OFF header.\n");
        fclose(fin);
        return 1;
    }

    printf("Vertices: %d\n", nv);
    printf("Faces: %d\n", nf);

    fclose(fin);


}
