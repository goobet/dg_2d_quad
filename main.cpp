#include <iostream>
#include "cmath"
#include "global.h"
/**
 * du/dt + Vx*du/dx + Vy*du/dy=0
 */

using namespace std;

const int N = 100;

const double H = 1.0/N;

const double TMAX = 10.0;
const double TAU = 1.e-2;

const int FILE_SAVE_STEP = 10;
const int PRINT_STEP = 1;

VECTOR u[N+2][N+2];
Point  c[N+2][N+2]; // центры ячеек

VECTOR u1[N+2][N+2]; // сюда суммируем интегралы

Point cellGP[4];
double cellGW[4] = {1, 1, 1, 1};

// матрица масс
double			****matrA;
double			****matrInvA;

int step = 0;
double t = 0.0;

const int FUNC_COUNT = 3;

double sqrt3 = 1.0/3.0;

VECTOR getF(int i, int j, Point pt);
VECTOR getDFDX(int iCell, Point pt);
VECTOR getDFDY(int iCell, Point pt);

inline double getU(int i, int j, Point pt) { return VECTOR::SCALAR_PROD(u[i][j], getF(i, j, pt)); }

void fillGaussPoints();
void calculateMassMatrix();
void calculateDoubleIntegral();
void calculateLineIntegral();
void calculateSolution();
void incrementTime();
void output();
void saveResult(char *fName);
void fillWithInitialData();
void calculateCellCenters();
int gaus_obr(int cnt_str, double **mass, double **M_obr);
void set_bounds();

int main() {
    // заполняем вспомогательные массивы
    calculateCellCenters();
    fillGaussPoints();
    calculateMassMatrix();
    fillWithInitialData();
    output();

    while (t < TMAX) {
        incrementTime();

        // Граничные условия
        set_bounds();

        // Правая часть уравнения
        calculateDoubleIntegral(); // вычисляем двойной интеграл
        calculateLineIntegral(); // вычисляем криволинейный интеграл по границе квадрата

        // Вычисляем значения решения в ячейках использую предыдущий слой и правую часть уравнения
        calculateSolution();

        // вывод через определенное количество шагов
        output();
    }
    // завершение приложения, очистка памяти..
    return 0;
}

void fillGaussPoints() {
    cellGP[0].x = -sqrt3;
    cellGP[0].y = -sqrt3;

    cellGP[1].x =  sqrt3;
    cellGP[1].y = -sqrt3;

    cellGP[2].x = sqrt3;
    cellGP[2].y = sqrt3;

    cellGP[3].x = -sqrt3;
    cellGP[3].y =  sqrt3;
    sqrt3 = 1.0/sqrt(3.0);
    // todo: cellGW ??????
}

void calculateMassMatrix() {
    matrA = new double***[N+2];
    matrInvA = new double***[N+2];
    for (int i = 0; i < N+2; i++) {
        matrA[i] = new double**[N+2];
        matrInvA[i] = new double**[N+2];
        for (int j = 0; j < N+2; j++) {
            matrA[i][j] = new double*[FUNC_COUNT];
            matrInvA[i][j] = new double*[FUNC_COUNT];
            for (int k = 0; k < FUNC_COUNT; k++) {
                matrA[i][j][k] = new double[FUNC_COUNT];
                matrInvA[i][j][k] = new double[FUNC_COUNT];
            }
        }
    }

    // вычисляем матрицу масс
    for (int i = 1; i <= N; i++) {
        for (int j = 1; j <= N; j++) {
            double **a = matrA[i][j];
            for (int m = 0; m < FUNC_COUNT; m++) {
                for (int n = 0; n < FUNC_COUNT; n++) {
                    a[m][n] = 0.0;
                }
            }
            for (int igp = 0; igp < 4; igp++) {
                Point gp = cellGP[igp];
                gp *= H/2; // todo: неоходимо изменить если не квадратные ячейки
                gp += c[i][j];
                VECTOR phi1 = getF(i,j, gp);
                VECTOR phi2 = getF(i,j, gp);
                for (int m = 0; m < FUNC_COUNT; m++) {
                    for (int n = 0; n < FUNC_COUNT; n++) {
                        a[m][n] += phi1[m] * phi2[n] *  cellGW[igp];
                    }
                }
            }

            gaus_obr(FUNC_COUNT, a, matrInvA[i][j]);
        }
    }
}

void calculateDoubleIntegral() {
    for (int i = 1; i < N+1; i++) {
        for (int j = 0; j < N+1; j++) {
            VECTOR s(FUNC_COUNT);
            s = 0.0;
            for (int igp = 0; igp < 4; igp++) {
                Point gp = cellGP[igp];
                gp *= H/2; // todo: неоходимо изменить если не квадратные ячейки
                gp += c[i][j];
                s += getU(i,j,gp)*(getDFDX(i,gp)+getDFDY(i,gp));
                s *= cellGW[igp];
            }

            u1[i][j] = s;
        }
    }
}

void lineIntegralXDirection() {
    for (int i = 0; i < N+1; i++) {
        for (int j = 1; j < N+1; j++) {
            Point gp1, gp2;
            gp1 = c[i][j];
            gp1.x += 0.5*H;
            gp2 = gp1;
            gp1.y -= sqrt3*H*0.5;
            gp2.y += sqrt3*H*0.5;
            double flux1 = getU(i, j, gp1);
            double flux2 = getU(i, j, gp2);

            VECTOR sL(FUNC_COUNT), sR(FUNC_COUNT), tmp(FUNC_COUNT);
            tmp = getF(i,j,gp1);
            tmp *= flux1;
            sL += tmp;
            tmp = getF(i,j,gp1);
            tmp *= flux2;
            sL += tmp;
            sL *= H*0.5;

            tmp = getF(i,j,gp2);
            tmp *= flux1;
            sR += tmp;
            tmp = getF(i,j,gp2);
            tmp *= flux2;
            sR += tmp;
            sR *= H*0.5;

            u1[i][j]   -= sL;
            u1[i+1][j] += sR;
        }
    }
}

void lineIntegralYDirection() {
    for (int i = 1; i < N+1; i++) {
        for (int j = 0; j < N+1; j++) {
            Point gp1, gp2;
            gp1 = c[i][j];
            gp1.y += 0.5*H;
            gp2 = gp1;
            gp1.x -= sqrt3*H*0.5;
            gp2.x += sqrt3*H*0.5;
            double flux1 = getU(i, j, gp1);
            double flux2 = getU(i, j, gp2);

            VECTOR sL(FUNC_COUNT), sR(FUNC_COUNT), tmp(FUNC_COUNT);
            tmp = getF(i,j,gp1);
            tmp *= flux1;
            sL += tmp;
            tmp = getF(i,j,gp1);
            tmp *= flux2;
            sL += tmp;
            sL *= H*0.5;

            tmp = getF(i,j,gp2);
            tmp *= flux1;
            sR += tmp;
            tmp = getF(i,j,gp2);
            tmp *= flux2;
            sR += tmp;
            sR *= H*0.5;

            u1[i][j]   -= sL;
            u1[i][j+1] += sR;
        }
    }
}

void calculateLineIntegral() {
    lineIntegralXDirection(); // x-direction

    lineIntegralYDirection(); // y-direction
}

void calculateSolution() {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            u1[i][j] *= matrInvA[i][j];
            u1[i][j] *= TAU;

            u[i][j] += u1[i][j];
        }
    }
}

VECTOR getF(int i, int j, Point pt) {
    if (FUNC_COUNT == 3 )
    {
        VECTOR v(3);
        v[0] = 1.0;
        v[1] = (pt.x - c[i][j].x)/H;
        v[2] = (pt.y - c[i][j].y)/H;
        return v;
    } else {
        std::cout << "ERROR: wrong basic functions count!\n";
        return 1;
    }
}

VECTOR getDFDX(int iCell, Point pt) {
    if (FUNC_COUNT == 3 )
    {
        VECTOR v(3);
        v[0] = 0.0;
        v[1] = 1.0/H;
        v[2] = 0.0;
        return v;
    } else {
        std::cout << "ERROR: wrong basic functions count!\n";
        return 1;
    }
}

VECTOR getDFDY(int iCell, Point pt) {
    if (FUNC_COUNT == 3 )
    {
        VECTOR v(3);
        v[0] = 0.0;
        v[1] = 0.0;
        v[2] = 1.0/H;
        return v;
    } else {
        std::cout << "ERROR: wrong basic functions count!\n";
        return 1;
    }
}

void output() {
    if (step % FILE_SAVE_STEP  == 0)
    {
        char fName[50];
        sprintf(fName, "res_%010d.csv", step);

        saveResult(fName);
    }

    if (step % PRINT_STEP == 0)
    {
        printf("step: %d\t\ttime step: %.16f\n", step, t);
    }
}

void saveResult(char *fName) {
    FILE * fp = fopen(fName, "w");

    fprintf(fp, "x,y,u\n");
    printf("File '%s' saved...\n", fName);

    for(int i = 1; i < N; i++) {
        for(int j = 1; j < N; j++) {
            double val = getU(i, j, c[i][j]);
            fprintf(fp, "%f,%f,%f\n", c[i][j].x, c[i][j].y, val);
        }
    }

    fclose(fp);
}

void incrementTime() {
    step++;
    t += TAU;
}

void fillWithInitialData() {
    const double r = 0.2;
    for (int i = 0; i <N+1; i++) {
        for (int j = 0; j < N+1; j++) {
            u[i][j] = VECTOR(3);
            u1[i][j] = VECTOR(3);

            Point lc = c[i][j];
            if ((lc.x-0.5)*(lc.x-0.5) + (lc.y-0.5)*(lc.y-0.5) < r*r)
            {
                u[i][j][0] = 10;
            }
        }
    }
}

void calculateCellCenters() {
    for (int i = 1; i < N; i++) {
        for (int j = 1; j < N; j++) {
            c[i][j] = Point(i*H + H/2, j*H + H/2);
        }
    }
}

int gaus_obr(int cnt_str, double **mass, double **M_obr) {
    int i, j, k;

    for (i = 0; i < cnt_str; i++) {
        for (j = 0; j < cnt_str; j++)
            M_obr[i][j] = 0;
        M_obr[i][i] = 1;
    }

    double a, b;
    for (i = 0; i < cnt_str; i++) {
        a = mass[i][i];
        for (j = i + 1; j < cnt_str; j++) {
            b = mass[j][i];
            for (k = 0; k < cnt_str; k++) {
                mass[j][k] = mass[i][k] * b - mass[j][k] * a;
                M_obr[j][k] = M_obr[i][k] * b - M_obr[j][k] * a;
            }
        }
    }

    double sum;
    for (i = 0; i < cnt_str; i++) {
        for (j = cnt_str - 1; j >= 0; j--) {
            sum = 0;
            for (k = cnt_str - 1; k > j; k--)
                sum += mass[j][k] * M_obr[k][i];
            M_obr[j][i] = (M_obr[j][i] - sum) / mass[j][j];
        }
    }
    return 1;
}

void set_bounds() {
    for (int i = 1; i < N; i++) {
        u[0][i] = u[N][i];
        u[N+1][i] = u[1][i];
        u[i][0] = u[i][N];
        u[i][N+1] = u[i][0];
    }
}