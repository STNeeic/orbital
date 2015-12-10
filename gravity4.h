#pragma once

#define WIDTH 100 //出力の横幅
#define HEIGHT 60 //出力の縦幅

#define SPACE_WIDTH 25 //トーラスの横幅 (0からx)
#define SPACE_HEIGHT 25 //トーラスの縦幅（0からy）

#define MAX_RADIUS 20
#define FREE 0

#define GOAL_M 16
typedef struct
{
    double x;
    double y;
} VELOSITY;

typedef struct
{
  double m;   // mass
  VELOSITY r;   // position
  VELOSITY v;  // velocity
} STAR;

typedef enum
    {
        NORMAL = 0,ATTRACTION = 1,REPULSION = -1
    } STATE;

typedef enum
    {
        OPENING,PLAYING,CLEAR,OUT
    } GAMESCENE;
void unify(int,int);
void crash(void);
void check_satstate(double);
void game_over(void);
