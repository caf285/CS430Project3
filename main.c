#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

// line for error
int line = 1;

// allocates large block of memory for structs
// union allows use of array of structs (seperate structs require array of pointers)
typedef struct {
    int kind; // 0 = camera, 1 = cylinder, 2 = sphere, 3 = plane
    double color[3];
    double height;
    double width;
    double radius;
    double position[3];
    double normal[3];
    double direction[3];
    double diffuseColor[3];
    double specularColor[3];
    double radialA0;
    double radialA1;
    double radialA2;
    double angularA0;
    double theta;
} Object;

// clamp
// returns value between 0 and 1
static inline double clamp (double color) {
    if (color < 0) {
        return 0;
    } else if (color > 1) {
        return 1;
    } else {
        return color;
    }
}

// squared^2 function
// make static for consistent behavior
static inline double sqr(double v){
    return v*v;
}

// exponent
// returns value x multiplied by itself by value y times
static inline double exponent(double x, double y){
    for (int i = 1; i < y; i++){
        x *= x;
    }
    return x;
}

// dot product
static inline double dot(double* x, double* y){
    return x[0] * y[0] + x[1] * y[1] + x[2] * y[2];
}

// distance
static inline double dist(double* x, double* y){
    return sqrt(sqr(y[0]-x[0])+sqr(y[1]-x[1])+sqr(y[2]-x[2]));
}

// normalize
static inline void normalize(double* v){
    double len = sqrt(sqr(v[0]) + sqr(v[1]) + sqr(v[2]));
    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
}

// (Ray Origin, Ray Direction, Center, Radius)
static inline double cylinderIntersection(double* Ro, double* Rd, double* C, double r) {
    /*
     ==> Step 1: Find equation for the object you are interested in (cylinder)
     x^2 + z*2 = r^2 (z axis makes cylider up and down)
     
     ==> Step 2: Parameterize the equation with a center point if needed
     (x-Cx)^2 + (z-Cz)^2 = r^
     
     ==> Step 3: Substitute the equation for a ray into our object equation
     (xRo + t*xRd - Cx)^2 + (xRo + t*xRd - Cz)^2 -r^2 = 0
     
     ==> Step 4: Solve for t
     => Step 4a: Rewrite equation (flatten)
     -r^2 +
     t^2 * xRd^2 +
     t^2 * zRd^2 +
     2*t * xRo * xRd -
     2*t * xRd * Cx +
     2*t * zRo * zRd -
     2*t * zRd * Cz +
     xRo^2 -
     2*xRo*Cx +
     Cx^2 +
     Roz^2 -
     2*Rox*Cz +
     Cz^2 = 0
     
     => Step 4b: Rewrite equation in terms of t
     t^2 (xRd^2 + zRd^2) +
     t (2 * (xRo * xRd - xRd * Cx + zRo * zRd - zRd * Cz)) +
     Rox^2 - 2*Rox*Cx + Cx^2 + Roz^2 - 2*Rox*Cz + Cz^2 = 0
     
     Use quadratic equation to solve for t
     */
    
    // double a = (Rdx^2 + Rdz^2)
    double a = (sqr(Rd[0]) + sqr(Rd[2]));
    double b = (2 * (Ro[0] * Rd[0] - Rd[0] * C[0] + Ro[2] * Rd[2] - Rd[2] * C[2]));
    double c = sqr(Ro[0]) - 2*Ro[0]*C[0] + sqr(C[0]) + sqr(Ro[2]) - 2*Ro[2]*C[2] + sqr(C[2]) - sqr(r);
    
    // quadratic equation
    double det = sqr(b) - 4 * a * c;
    if (det < 0) return -1;
    
    det = sqrt(det);
    
    // return lowest t
    double t0 = (-b - det) / (2*a);
    double t1 = (-b + det) / (2*a);
    if (t0 > 0 || t1 > 0){
        if (t0 < t1) return t0;
        return t1;
    }
    return -1;
}

// (Ray Origin, Ray Direction, Center, Radius)
static inline double sphereIntersection(double* Ro, double* Rd, double* C, double r) {
    
    // intersection
    double a = sqr(Rd[0])+sqr(Rd[1])+sqr(Rd[2]);
    double b = 2*(Rd[0]*(Ro[0]-C[0])+Rd[1]*(Ro[1]-C[1])+Rd[2]*(Ro[2]-C[2]));
    double c = sqr(Ro[0]-C[0])+sqr(Ro[1]-C[1])+sqr(Ro[2]-C[2])-sqr(r);
    
    // determinant
    double det = sqr(b) - 4 * a * c;
    
    // quadratic
    det = sqrt(det);
    double t0 = (-b - det) / (2*a);
    double t1 = (-b + det) / (2*a);
    
    // return lowest t
    if (t0 > 0 || t1 > 0){
        if (t0 < t1) return t0;
        return t1;
    }
    return -1;
}

// (Ray Origin, Ray Direction, Center, Radius)
static inline double planeIntersection(double* Ro, double* Rd, double* C, double* n) {
    
    // set center and origin for dot product
    double l[] = {C[0]-Ro[0], C[1]-Ro[1], C[2]-Ro[2]};
    
    // dot top and bottom
    double numerator = dot(l, n);
    double denominator = dot(Rd, n);
    
    // error if div by 0
    if (denominator == 0){
        fprintf(stderr, "Error: Illegal plane. Cannot divide by zero.\n");
        exit(1);
    }
    
    // find t and return
    double t = numerator / denominator;
    if (t > 0) return t;
    return -1;
}

// frad function
double frad(double a2, double a1, double a0, double dist){
    double denominator = a2*dist+a1*dist+a0;
    if (denominator == 0){
        fprintf(stderr, "Error: Cannot divide by zero.\n");
        exit(1);
    }
    return 1/denominator;
}

// fang function
double fang(double theta, double* lightDirection, double* Ron, double angularA0){
    theta = theta * (M_PI / 180);
    double cosTheta = cos(theta);
    double cosAlpha = dot(lightDirection, Ron);
    if (cosAlpha < cosTheta) return 0.0;
    return exponent(cosAlpha, angularA0);
}

// nextC
int nextC(FILE* json) {
    int c = fgetc(json);
    /*#ifdef DEBUG
     printf("nextC: '%c'\n", c);
     #endif*/
    if (c == '\n') {
        line += 1;
    }
    if (c == EOF) {
        fprintf(stderr, "Error: Unexpected end of file on line number %d.\n", line);
        exit(1);
    }
    return c;
}

// expectC (pass in FILE, and expected character) (fail if not expected)
void expectC(FILE* json, int d) {
    int c = nextC(json);
    if (c == d){
        return;
    } else {
        fprintf(stderr, "Error: Expected '%c' on line %d.\n", d, line);
        exit(1);
    }
}

// reads ahead in file until no whitespace
void skipWS(FILE* json) {
    int c = fgetc(json);
    while (isspace(c)){
        
        if (c == '\n'){
            line ++;
        }
        c = fgetc(json);
    }
    ungetc(c, json); //backtracks pointer in file
}

// reads string buffer until end of string
char* nextString(FILE* json) {
    char buffer[129];
    int c = nextC(json);
    if (c != '"') {
        fprintf(stderr, "Error: Expected string on line %d.\n", line);
        exit(1);
    }
    c = nextC(json);
    int i = 0;
    while (c != '"') {
        if (i >= 128) {
            fprintf(stderr, "Error: Strings longer than 128 characters in length are not supported.\n");
            exit(1);
        }
        if (c == '\\') {
            fprintf(stderr, "Error: Strings with escape codes are not supported.\n");
            exit(1);
        }
        if (c < 32 || c > 126) {
            fprintf(stderr, "Error: Strings may contain only ascii characters.\n");
            exit(1);
        }
        buffer[i] = c;
        i += 1;
        c = nextC(json);
    }
    buffer[i] = 0;
    /*#ifdef DEBUG
     printf("%s\n", buffer);
     #endif*/
    return strdup(buffer);
}

// return next number
float nextNumber(FILE* json) {
    float value;
    fscanf(json, "%f", &value);
    //Error check this..
    return value;
}

// return next vector
float* nextVector(FILE* json){
    float* v = malloc(sizeof(float)*3);
    expectC(json, '[');
    skipWS(json);
    v[0] = nextNumber(json);
    skipWS(json);
    expectC(json, ',');
    skipWS(json);
    v[1] = nextNumber(json);
    skipWS(json);
    expectC(json, ',');
    skipWS(json);
    v[2] = nextNumber(json);
    skipWS(json);
    expectC(json, ']');
    return v;
}

// readScene (open and parse json file)
Object** readScene(char* fileName){
    Object** objects = malloc(sizeof(Object)*256);
    
    int c;
    FILE* json = fopen(fileName, "r");
    
    if (json == NULL){
        fprintf(stderr, "Error: Could not open file \"%s\"\n", fileName);
    }
    
    skipWS(json);
    expectC(json, '[');
    skipWS(json);
    
    // objects[i];
    int i = 0;
    
    // Find the objects
    while (1) {
        objects[i] = malloc(sizeof(Object));
        c = fgetc(json);
        if (c == ']') {
            fprintf(stderr, "Error: Json file contains no data.\n");
            fclose(json);
            exit(1);
        }
        if (c == '{') {
            skipWS(json);
            
            // Parse the object
            char* key = nextString(json);
            if (strcmp(key, "type") != 0) {
                fprintf(stderr, "Error: Expected \"type\" key on line number %d.\n", line);
                exit(1);
            }
            
            skipWS(json);
            
            expectC(json, ':');
            
            skipWS(json);
            
            char* value = nextString(json);
            
            if (strcmp(value, "camera") == 0) {
                objects[i]->kind = 0;
            } else if (strcmp(value, "cylinder") == 0) {
                objects[i]->kind = 1;
            } else if (strcmp(value, "sphere") == 0) {
                objects[i]->kind = 2;
            } else if (strcmp(value, "plane") == 0) {
                objects[i]->kind = 3;
            } else if (strcmp(value, "light") == 0) {
                objects[i]->kind = 4;
            } else {
                fprintf(stderr, "Error: Unknown type, \"%s\", on line number %d.\n", value, line);
                exit(1);
            }
            
            skipWS(json);
            
            while (1) {
                // , }
                c = nextC(json);
                if (c == '}') {
                    // stop parsing this object
                    break;
                } else if (c == ',') {
                    // read another field
                    skipWS(json);
                    char* key = nextString(json);
                    skipWS(json);
                    expectC(json, ':');
                    skipWS(json);
                    
                    // assign all object values
                    if ((strcmp(key, "width") == 0) ||
                        (strcmp(key, "height") == 0) ||
                        (strcmp(key, "radius") == 0) ||
                        (strcmp(key, "radial-a0") == 0) ||
                        (strcmp(key, "radial-a1") == 0) ||
                        (strcmp(key, "radial-a2") == 0) ||
                        (strcmp(key, "angular-a0") == 0) ||
                        (strcmp(key, "theta") == 0)) {
                        float value = nextNumber(json);
                        if (strcmp(key, "width") == 0){
                            objects[i]->width = value;
                            
                        } else if (strcmp(key, "height") == 0){
                            objects[i]->height = value;
                        } else if (strcmp(key, "radius") == 0){
                            objects[i]->radius = value;
                        } else if (strcmp(key, "radial-a0") == 0){
                            objects[i]->radialA0 = value;
                        } else if (strcmp(key, "radial-a1") == 0){
                            objects[i]->radialA1 = value;
                        } else if (strcmp(key, "radial-a2") == 0){
                            objects[i]->radialA2 = value;
                        } else if (strcmp(key, "angular-a0") == 0){
                            objects[i]->angularA0 = value;
                        } else if (strcmp(key, "theta") == 0){
                            objects[i]->theta = value;
                        }
                    } else if ((strcmp(key, "color") == 0) ||
                               (strcmp(key, "position") == 0) ||
                               (strcmp(key, "normal") == 0) ||
                               (strcmp(key, "direction") == 0) ||
                               (strcmp(key, "diffuse_color") == 0) ||
                               (strcmp(key, "specular_color") == 0)) {
                        
                        float* value = nextVector(json);
                        if (strcmp(key, "color") == 0){
                            objects[i]->color[0] = value[0];
                            objects[i]->color[1] = value[1];
                            objects[i]->color[2] = value[2];
                        } else if (strcmp(key, "position") == 0) {
                            objects[i]->position[0] = value[0];
                            objects[i]->position[1] = value[1];
                            objects[i]->position[2] = value[2];
                        } else if (strcmp(key, "normal") == 0) {
                            objects[i]->normal[0] = value[0];
                            objects[i]->normal[1] = value[1];
                            objects[i]->normal[2] = value[2];
                        } else if (strcmp(key, "direction") == 0) {
                            objects[i]->direction[0] = value[0];
                            objects[i]->direction[1] = value[1];
                            objects[i]->direction[2] = value[2];
                        } else if (strcmp(key, "diffuse_color") == 0) {
                            objects[i]->diffuseColor[0] = value[0];
                            objects[i]->diffuseColor[1] = value[1];
                            objects[i]->diffuseColor[2] = value[2];
                        } else if (strcmp(key, "specular_color") == 0) {
                            objects[i]->specularColor[0] = value[0];
                            objects[i]->specularColor[1] = value[1];
                            objects[i]->specularColor[2] = value[2];
                        }
                        
                    } else {
                        fprintf(stderr, "Error: Unknown property, \"%s\", on line %d.\n",
                                key, line);
                        //char* value = next_string(json);
                    }
                    skipWS(json);
                } else {
                    fprintf(stderr, "Error: Unexpected value on line %d\n", line);
                    exit(1);
                }
            }
            i++;
            
            skipWS(json);
            c = nextC(json);
            if (c == ',') {
                // noop
                skipWS(json);
            } else if (c == ']') {
                fclose(json);
                objects[++i] = NULL;
                return objects;
            } else {
                printf("---------------------------------");
                fprintf(stderr, "Error: Expecting ',' or ']' on line %d.\n", line);
                exit(1);
            }
        }
    }
}

// write P6 header to buffer
char* buildHeader(Object** objects, int M, int N){
    char* header = malloc(sizeof(char)*50);
    char* headerNode = header;
    sprintf(headerNode, "P6\n%d\n%d\n255\n", M, N);
    
    return header;
}

// build image buffer based on objects
unsigned char* buildBuffer(Object** objects, int M, int N){
    
    // build lights objects
    Object** lights = malloc(sizeof(Object)*128);
    int lightCount = 0;
    for (int i=1; objects[i] != 0; i ++) {
        switch(objects[i]->kind) {
            case 0:
                break;
            case 1:
                break;
            case 2:
                break;
            case 3:
                break;
            case 4:
                lights[lightCount] = objects[i];
                lightCount++;
                break;
            default:
                break;
        }
    }
    if (lightCount == 0){
        fprintf(stderr, "Error: No lights were found in scene");
        exit(1);
    }
    
    // camera center
    double cx = 0;
    double cy = 0;
    
    // camera width and height
    double h = objects[0]->height;
    double w = objects[0]->width;
    
    // open output file && write header
    unsigned char* buffer = malloc(sizeof(char)*M*N*10);
    unsigned char* bufferNode = buffer;
    
    // space for single pixel
    double* Ro = malloc(sizeof(double)*3);
    double* Rd = malloc(sizeof(double)*3);
    
    // build scene
    double pixheight = h / M;
    double pixwidth = w / N;
    for (int y = M; y >0; y -= 1) {
        for (int x = 0; x < N; x += 1) {
            
            // space for single pixel
            Ro[0] = 0;
            Ro[1] = 0;
            Ro[2] = 0;
            
            // Rd = normalize(P - Ro)
            Rd[0] = cx - (w/2) + pixwidth * (x + 0.5);
            Rd[1] = cy - (h/2) + pixheight * (y + 0.5);
            Rd[2] = 1;
            normalize(Rd);
            
            // paint pixel based on type
            double closestT = INFINITY;
            Object* closestObject = NULL;
            
            // create color list
            double* color = malloc(sizeof(double)*3);
            color[0] = 0; // ambient_color[0];
            color[1] = 0; // ambient_color[1];
            color[2] = 0; // ambient_color[2];
            
            for (int i=1; objects[i] != 0; i ++) {
                double t = 0;
                
                // find closest intersection based on objects
                switch(objects[i]->kind) {
                    case 0:
                        break;
                    case 1:
                        t = cylinderIntersection(Ro, Rd, objects[i]->position, objects[i]->radius);
                        if (t > 0 && t < closestT){
                            closestT = t;
                            closestObject = objects[i];
                        }
                        break;
                    case 2:
                        t = sphereIntersection(Ro, Rd, objects[i]->position, objects[i]->radius);
                        if (t > 0 && t < closestT){
                            closestT = t;
                            closestObject = objects[i];

                        }
                        break;
                    case 3:
                        t = planeIntersection(Ro, Rd, objects[i]->position, objects[i]->normal);
                        if (t > 0 && t < closestT){
                            closestT = t;
                            closestObject = objects[i];
                        }
                        break;
                    case 4:
                        break;
                    default:
                        fprintf(stderr, "Error: Invalid type number: %i", objects[i]->kind);
                        exit(1);
                        break;
                }
            }
            
            if (closestT < INFINITY){
                // discover lights
                for (int j = 0; lights[j] !=NULL; j++){
                    
                    // new origin
                    double* Ron = malloc(sizeof(double)*3);
                    Ron[0] = closestT * Rd[0] + Ro[0];
                    Ron[1] = closestT * Rd[1] + Ro[1];
                    Ron[2] = closestT * Rd[2] + Ro[2];
                    
                    // new direction
                    double* Rdn = malloc(sizeof(double)*3);
                    Rdn[0] = lights[j]->position[0] - Ron[0];
                    Rdn[1] = lights[j]->position[1] - Ron[1];
                    Rdn[2] = lights[j]->position[2] - Ron[2];
                    
                    double closestT = INFINITY;
                    Object* closestShadowObject = NULL;
                    
                    for (int k = 0; objects[k] != NULL; k++){
                        
                        if (objects[k] == closestObject){
                            continue;
                        }
                        double t = 0;
                        
                        
                        // object->intersect()
                        switch(objects[k]->kind){
                            case 0:
                                break;
                            case 1:
                                t = cylinderIntersection(Ron, Rdn, objects[k]->position, objects[k]->radius);
                                if (t > 0 && t < closestT){
                                    closestT = t;
                                    
                                }
                                break;
                            case 2:
                                t = sphereIntersection(Ron, Rdn, objects[k]->position, objects[k]->radius);
                                if (t > 0 && t < closestT){
                                    closestT = t;
                                }
                                break;
                            case 3:
                                t = planeIntersection(Ron, Rdn, objects[k]->position, objects[k]->normal);
                                if (t > 0 && t < closestT){
                                    closestT = t;
                                }
                                break;
                            case 4:
                                break;
                            default:
                                fprintf(stderr, "Error: Invalid type number: %i", objects[k]->kind);
                                exit(1);
                                break;
                        
                        }
                        
                        if (closestT < INFINITY && closestT < dist(Ron, lights[j]->position)){
                            closestShadowObject = objects[k];
                        } else {
                            continue;
                        }
                        
                        
                    }
                    
                    if (closestShadowObject == NULL) {
                        // N, L, R, V
                        // shinyness
                        double NS = 7;
                        
                        // N
                        double* N = malloc(sizeof(double)*3);
                        switch(closestObject->kind){
                            case 2: // sphere
                                N[0] = Ron[0] - closestObject->position[0];
                                N[1] = Ron[1] - closestObject->position[1];
                                N[2] = Ron[2] - closestObject->position[2];
                                normalize(N);
                                break;
                            case 3: // plane
                                N = closestObject->normal;
                                normalize(N);
                                break;
                            default:
                                break;
                        }
                        
                        // L
                        double* L = malloc(sizeof(double)*3);
                        L = Rdn; // light_position - Ron;
                        normalize(L);
                        
                        // R = reflection of L
                        double* R = malloc(sizeof(double)*3);
                        
                        R[0] = 2 * N[0] * dot(N, L) - L[0];
                        R[1] = 2 * N[1] * dot(N, L) - L[1];
                        R[2] = 2 * N[2] * dot(N, L) - L[2];
                        
                        // V = Rd;
                        double* V = malloc(sizeof(double)*3);
                        V[0] = -1 * Rd[0];
                        V[1] = -1 * Rd[1];
                        V[2] = -1 * Rd[2];
                        
                        // diffuse
                        double* diffuse = malloc(sizeof(double)*3);
                        if (dot(N, L) > 0){
                            diffuse[0] = closestObject->diffuseColor[0] * lights[j]->color[0] * dot(N, L);
                            diffuse[1] = closestObject->diffuseColor[1] * lights[j]->color[0] * dot(N, L);
                            diffuse[2] = closestObject->diffuseColor[2] * lights[j]->color[0] * dot(N, L);
                        } else {
                            diffuse[0] = 0;
                            diffuse[1] = 0;
                            diffuse[2] = 0;
                        }
                        
                        // specular
                        double* specular = malloc(sizeof(double)*3);
                        specular[0] = 0; // uses object's specular color
                        specular[1] = 0;
                        specular[2] = 0;
                        if (dot(V, R) > 0 && dot(N, L) > 0){
                            specular[0] = closestObject->specularColor[0] * lights[j]->color[0] * exponent(dot(R, V), NS); // uses object's specular color
                            specular[1] = closestObject->specularColor[1] * lights[j]->color[1] * exponent(dot(R, V), NS);
                            specular[2] = closestObject->specularColor[2] * lights[j]->color[2] * exponent(dot(R, V), NS);
                            } else {
                            specular[0] = 0; // uses object's specular color
                            specular[1] = 0;
                            specular[2] = 0;

                        }
                        if (lights[j]->direction[0] == 0 && lights[j]->direction[1] == 0 && lights[j]->direction[2] == 0){
                            color[0] += frad(lights[j]->radialA2, lights[j]->radialA1, lights[j]->radialA0, dist(Ron, lights[j]->position)) * (diffuse[0] + specular[0]);
                            color[1] += frad(lights[j]->radialA2, lights[j]->radialA1, lights[j]->radialA0, dist(Ron, lights[j]->position)) * (diffuse[1] + specular[1]);
                            color[2] += frad(lights[j]->radialA2, lights[j]->radialA1, lights[j]->radialA0, dist(Ron, lights[j]->position)) * (diffuse[2] + specular[2]);
                        } else {
                            color[0] += frad(lights[j]->radialA2, lights[j]->radialA1, lights[j]->radialA0, dist(Ron, lights[j]->position)) * fang(lights[j]->theta, lights[j]->direction, Ron, lights[j]->angularA0) * (diffuse[0] + specular[0]);
                            color[1] += frad(lights[j]->radialA2, lights[j]->radialA1, lights[j]->radialA0, dist(Ron, lights[j]->position)) * fang(lights[j]->theta, lights[j]->direction, Ron, lights[j]->angularA0) * (diffuse[1] + specular[1]);
                            color[2] += frad(lights[j]->radialA2, lights[j]->radialA1, lights[j]->radialA0, dist(Ron, lights[j]->position)) * fang(lights[j]->theta, lights[j]->direction, Ron, lights[j]->angularA0) * (diffuse[2] + specular[2]);
                        }
                        
                    } else {
                        color[0] /=5;
                        color[1] /=5;
                        color[2] /=5;
                    }
                }
            }
            
            *bufferNode++ = (255 * clamp(color[0]));
            *bufferNode++ = (255 * clamp(color[1]));
            *bufferNode++ = (255 * clamp(color[2]));
        }
    }
    
    // end buffer
    *bufferNode = '\0';
    return buffer;
}

// open new file and dump image buffer
void buildFile(char* header, unsigned char* buffer, char* fileName, int M, int N){
    FILE *FH = fopen(fileName, "w+");
    char* headerNode = header;
    unsigned char* bufferNode = buffer;
    
    while (*headerNode){
        fputc(*headerNode++, FH);
    }
    
    for (int i = 0; i < (M * N * 3); i++){
        fputc(*bufferNode++, FH);
    }
    fputc(EOF, FH);
    fclose(FH);
}

int main(int argc, char* argv[]) {
    
    // scene width and height
    int M = atoi(argv[1]);
    int N = atoi(argv[2]);
    
    // read json, and build objects
    Object** objects = readScene(argv[3]);
    
    // build header buffer
    char* header = buildHeader(objects, M, N);
    
    // build image buffer
    unsigned char* buffer = buildBuffer(objects, M, N);
    
    // dump buffer to file
    buildFile(header, buffer, argv[4], M, N);
    return 0;
}
