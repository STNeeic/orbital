#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "gravity4.h"
#include "rsc/kbhit.h"

/*Orbitalが作りたい!!*/
/*Orbitalとは、ニンテンドーのGBAで発売されたbitgenerationsシリーズの中の一つのタイトルで
  その名の通り、自分が惑星となって引力と斥力を駆使して星々を飲み込むゲームなのだ!
  今回はとりあえず自分の惑星の引力と斥力による移動を実装するぞ*/

/* TODO
   1:自分の惑星の引力斥力による操作を記述する
   2:それを出力できるように変える
   3:惑星の吸収が出来るようにする
   4:座標をトーラスに変える。引力の計算に結構な支障が出るが、力の及ぶ範囲を限定すれば問題ない
   6:出力を自分が中心になるように変更する。そして投射する範囲を狭めて「大きく」星を映す
   7:質量に合わせて、白星、青星、赤星を作る。つまり、衝突する際に質量ごとに違った判定を出すようにする
   8:自分と赤い星、自分と白い星の間で衛生判定ができるようにする
   9:ゲームの最初の画面、ゲーム中、終了条件(ゴール) (出来れば)次の面への遷移を作成する
*/

/*衛星とは
  ある星の周りを周回して移動するようになる状態。
  自分が赤い星の周りを周回するときには、引力と斥力はその星との間でしか働かず、つまり集会する円周の長さ方向の変化しかないようになる
  そして特定の距離を超えたらその関係が終わる。その間は、速さは変化せず、どんなに吸引してもその半径より近くには移動しない。*/
/*トーラスを作成するには
  自機の位置は区間を超えたら反対側に出せば良い(これはx,y別個に判定を作成する)
  星は、本来の画面の周囲8面を仮想的に全く同じ面で囲えば良い。
  つまり、 VVV
          VRV
          VVV
          R:自機がいる世界。V:自機がいない世界とする。
          惑星の吸収の問題があるので、毎回力判定の時にミラーリングすれば良い
*/

const double G = 2.0;  // gravity constant
const double COLLISION_R = 1.0;
const double CHAR_ASPECT = 13.0 / 8.0; //文字の縦横の幅
const double PRINT_HEIGHT = 30.0 / CHAR_ASPECT; //表示する部分の縦の(実際の計算上の領域での)幅
const double SATELITE_TIME = 1.0; //衛星になるのに必要な時間
/*stars[0] を特別に自機として扱う*/
STAR *stars = NULL;
int nstars = -1;
STATE mystate = NORMAL; //引力か斥力かを扱う。引力なら1,斥力なら-1,平常時なら0になる
int satstate = FREE;

VELOSITY add_vel(VELOSITY a,VELOSITY b)
{
    VELOSITY out = {a.x + b.x, a.y + b.y};
    
    return out;
}

VELOSITY multi_vel(VELOSITY a, double b)
{
    VELOSITY out = {a.x * b, a.y * b};
    return out;
}


VELOSITY div_vel(VELOSITY a, double b)
{
    VELOSITY out = {a.x / b,a.y / b};
    return out;
}

VELOSITY new_vel(double x,double y)
{
    VELOSITY out = {x,y};
    return out;
}

double pow_vel(VELOSITY a, VELOSITY b,int n)
{
    VELOSITY diff = {a.x - b.x , a.y - b.y};
    double l = sqrt(pow(diff.x,2) + pow(diff.y,2));

    return pow(l , n);
}

double rot_vel(VELOSITY a,VELOSITY b) //AxBの値を出力する。主に回転方向の推定に用いる
{
    return a.x * b.y - a.y * b.x;
}

VELOSITY rotate_vel(double theta,VELOSITY a)
{
    return new_vel(cos(theta) * a.x + sin(theta) * a.y, -sin(theta) * a.x + cos(theta) * a.y);
}

FILE *stack_fp(FILE *fp)
{
    static FILE* out = NULL;
    if(fp == NULL)
        {
            if(out == NULL)
                {
                    fprintf(stderr,"error: No fp on the stack\n");
                    exit(1);
                }
            else
                return out;
        }
    else
        {
            out = fp;
            return NULL;
        }
}

void plot_stars(FILE *fp)
{
  char space[WIDTH][HEIGHT];
  //自分の星を中心として、縦に10ぐらいを表示したい
  VELOSITY o = new_vel(stars[0].r.x, stars[0].r.y); //この位置を常に中心に表示する

  VELOSITY mirrors[9] = {};
  for(int j = 0; j < 3; j++)
      {
          for(int k = 0; k < 3; k++)
              {
                  mirrors[j*3 + k] = new_vel((j - 1) *2 * SPACE_WIDTH , (k - 1) * 2 * SPACE_HEIGHT);
              }
      }

  memset(space, ' ', sizeof(space));
  for(int i = 0; i < WIDTH ;i++)
      {
          for(int j = 0; j < HEIGHT;j++)
              {
                  double dy = ((j - HEIGHT/2) / (double) HEIGHT) * PRINT_HEIGHT * CHAR_ASPECT;
                  double dx = ((i - WIDTH/2) / (double) WIDTH) * PRINT_HEIGHT * ((double) WIDTH / (double) HEIGHT); //アスペクト調整
                  
                  const VELOSITY r = add_vel(o, new_vel(dx,dy));

                  //printf("%d %d %f %f #r\n",i, j, r.x, r.y);
                  
                  for(int k = 0; k < nstars * 9; k++)
                      {
                          if(pow_vel(r,add_vel(stars[k % nstars].r, mirrors[k/nstars]), 1) <= COLLISION_R * cbrt(stars[k % nstars].m))
                              {
                                  switch(k % nstars)
                                      {
                                      case 0: //自機
                                           switch(mystate)
                                              {
                                              case NORMAL:
                                                  space[i][j] = '@';
                                                  break;
                                              case ATTRACTION:
                                                  space[i][j] = '#';
                                                  break;
                                              case REPULSION:;
                                                  space[i][j] = '$';
                                                  break;
                                              }
                                          break;
                                      default: //それ以外 (もしも岩とかがあるなら変わる)
                                          if(stars[k % nstars].m > stars[0].m)
                                              {
                                                  space[i][j] = 'R';
                                              }
                                          else if(stars[k % nstars].m == stars[0].m)
                                              {
                                                  space[i][j] = 'B';
                                              }
                                          else
                                              {
                                                  space[i][j] = 'O';
                                              }
                                         
                                      }
                              }
                      }
              }
      }
  

  int x, y;
  for(int i = 0; i < WIDTH;i++)fprintf(fp, "-");
  fprintf(fp,"\n");
  
  for (y = 0; y < HEIGHT; y++) {
    for (x = 0; x < WIDTH; x++)
      fputc(space[x][y], fp);
    fputc('\n', fp);
  }
  fflush(fp);
}

void plot_status(const double t)
{
    printf("t = %5.1f", t);
    printf("x = %5.2f y = %5.2f vx = %5.2f vy = %5.2f",stars[0].r.x ,stars[0].r.y, stars[0].v.x,stars[0].v.y);
  printf("\n");
}


VELOSITY calc_gravity(double mass,VELOSITY target,VELOSITY b)
{ 
    VELOSITY out = {};
    out.x = mystate * G * mass * (b.x - target.x)/ pow_vel(b,target,3);
    out.y = mystate * G * mass * (b.y - target.y)/ pow_vel(b,target,3);

    return out;
}

void runge_kutta(const double dt) //ルンゲ--クッタ法を用いる(RK4)
{
  int j;
 
  VELOSITY a[4] = {},v[4] = {}, x[3];

  v[0] = multi_vel(stars[0].v , dt);

          VELOSITY mirrors[9] = {};
          for(j = 0; j < 3; j++)
              {
                  for(int k = 0; k < 3; k++)
                      {
                          mirrors[j*3 + k] = new_vel((j - 1) *2 * SPACE_WIDTH , (k - 1) * 2 * SPACE_HEIGHT);
                      }
              }
                  
          for(j = 0; j < nstars * 9; j++) //ミラーされた惑星についても考慮するので
              {
                  if(j % nstars != 0)
                      {
                          if(pow_vel(stars[0].r,add_vel(stars[j % nstars].r , mirrors[j/nstars]), 1) <= MAX_RADIUS)
                              { //力の及ぶ範囲内にいたら
                                  a[0] = add_vel(a[0], calc_gravity(stars[0].m, stars[0].r,add_vel(stars[j % nstars].r ,mirrors[j/nstars])));
                              }
                                  
                      }
              }
          a[0] = multi_vel(a[0],dt);
                  
                  
          v[1] = multi_vel(add_vel(stars[0].v,div_vel(a[0], 2.0)),dt);
          x[0] = add_vel(stars[0].r,div_vel(v[0], 2.0));
          
          for(j = 0; j < nstars * 9; j++)
              {
                  if(j % nstars != 0)
                      {
                          if(pow_vel(stars[0].r,add_vel(stars[j % nstars].r , mirrors[j/nstars]), 1) <= MAX_RADIUS) //力の及ぶ範囲内にいたら
                              {                                      
                                  a[1] = add_vel(a[1], calc_gravity(stars[0].m, x[0], add_vel(stars[j % nstars].r ,mirrors[j/nstars])));
                              }
                      }
              }
          a[1] = multi_vel(a[1],dt);
          
          v[2] = multi_vel(add_vel(stars[0].v,div_vel(a[1], 2.0)),dt);
          x[1] = add_vel(stars[0].r,div_vel(v[1],2.0));
          
          for(j = 0; j < nstars * 9; j++)
              {
                  if(j % nstars != 0)
                      {
                          if(pow_vel(stars[0].r,add_vel(stars[j % nstars].r , mirrors[j/nstars]), 1) <= MAX_RADIUS) //力の及ぶ範囲内にいたら
                              {
                                  a[2] = add_vel(a[2], calc_gravity(stars[0].m, x[1], add_vel(stars[j % nstars].r ,mirrors[j/nstars])));
                              }
                      }
              }
          a[2] = multi_vel(a[2],dt);

          v[3] = multi_vel(add_vel(stars[0].v,a[2]),dt);
          x[2] = add_vel(stars[0].r,v[2]);
          for(j = 0; j < nstars * 9; j++)
              {
                  if(j % nstars != 0)
                      {
                          if(pow_vel(stars[0].r,add_vel(stars[j % nstars].r , mirrors[j/nstars]), 1) <= MAX_RADIUS) //力の及ぶ範囲内にいたら
                              {
                                  a[3] = add_vel(a[3], calc_gravity(stars[0].m, x[2], add_vel(stars[j % nstars].r ,mirrors[j/nstars])));
                              }
                      }
              }
          a[3] = multi_vel(a[3],dt);
          //printf("%f %f #a[3]\n",a[3].x, a[3].y); //for debug
          stars[0].r = add_vel(stars[0].r,div_vel(add_vel(v[0], add_vel( multi_vel(v[1],2.0), add_vel( multi_vel(v[2], 2.0) ,v[3]))) , 6.0));
          stars[0].v = add_vel(stars[0].v,div_vel(add_vel(a[0], add_vel( multi_vel(a[1],2.0), add_vel( multi_vel(a[2], 2.0) ,a[3]))) , 6.0));

}

void move_other_stars(const double dt)
{
    for(int i = 1; i < nstars; i++)
        {
            stars[i].r = add_vel(stars[i].r , multi_vel(stars[i].v , dt));
        }
    
}


void detect_collision(void)
{

    for(int i = 1; i < nstars; i++)
        {

            if(pow_vel(stars[0].r,stars[i].r,1) <= COLLISION_R * (cbrt(stars[0].m) + cbrt(stars[i].m))) //半径は質量の3乗根に比例するはず
                {
                    if(stars[0].m == stars[i].m)
                        {
                            unify(0,i); //同じ大きさの星ならぶつかって大きくなる
                        }
                    else if(stars[0].m < stars[i].m)
                        {
                            crash(); //大きい星にぶつかったら衝突してゲーム終了
                        }
                    else
                        {
                            //相手の方が小さかった時は何もおこらない。ただ相手の星が消えるだけ
                        }
                    
                    
                    for(int k = i;k - 1< nstars; k++)
                        {
                            stars[k] = stars[k + 1];
                        }
                    nstars--;
                }
        }
}

double dot_line_dist(double a, double b, double c, double x, double y) //点と直線の距離を求める
{
    return fabs(a * x + b * y + c) / sqrt(a * a + b * b);
}

double my_star_dist(STAR my,STAR target)
{
    return dot_line_dist(my.v.y, -1 * my.v.x, (my.v.x * my.r.y - my.v.y * my.r.x), target.r.x, target.r.y);
}

void check_satstate(const double dt)
{
    //ここは自分が衛星になるか、他の星が衛星になるかを判定する所
    static double time = 0;
    static int kind = 0;
    //まずは赤い星について見ていく
    int flg = 0; //どこかの星の近くにいるかのフラグ
    for(int i = 1; i < nstars; i++)
        {
            if(stars[i].m > stars[0].m) //大きい星で
                {
                    if(pow_vel(stars[0].r,stars[i].r,1) <= COLLISION_R * cbrt(stars[i].m) * 2) //軌道内にいる
                        {
                            flg = 1;
                            if(kind != i && satstate == FREE){
                                kind = i, time = 0; //その星に照準を合わせる
                                printf("attempting to lift on the satellite orbit\n");
                            }
                            
                            if(my_star_dist(stars[0],stars[i]) > COLLISION_R *cbrt(stars[0].m) + cbrt(stars[i].m))
                                {
                                    time += dt; //衝突しない
                                    if(time < SATELITE_TIME)
                                        {
                                            printf(".");
                                        }
                                    
                                }
                            
                            if(time > SATELITE_TIME + dt) //一定時間いたらその星の衛星になる。
                                {
                                    satstate = kind;
                                }

                            else if(time > SATELITE_TIME)
                                {
                                    satstate = kind;
                                    printf("succesfully lifted on the satellite orbit\n");
                                }
                            
                            else satstate = FREE; //いないなら自由の身
                            
                        }
                }
        }
    
    if(flg == 0) satstate = FREE, kind = 0, time = 0; //自由だったら自由にしてやる
    
    //他の星の絵性判定はとりあえずいいや

}

void on_the_satellite_orbit(const double dt)
{
    //satstateの部分の球の周囲を周回する。
    //rの接線方向の単位ベクトルnを求める
    VELOSITY r = new_vel(stars[satstate].r.x - stars[0].r.x, stars[satstate].r.y - stars[0].r.y);
    double lr = sqrt(r.x * r.x + r.y * r.y);
    VELOSITY n = multi_vel(new_vel(-1 * r.y, r.x), 1.0/lr);

    const double v = stars[0].v.x * n.x + stars[0].v.y * n.y; //接線方向の速度の大きさ
    //このvを維持してsatstateの周りを回る。aやsの状態であれば半径の増減も行う
    const double omega = (rot_vel(n, r) > 0 )? v * dt / lr : -1 * v * dt / lr;
    stars[0].r = add_vel(stars[0].r, multi_vel(rotate_vel(M_PI_2 - omega/2.0, multi_vel(r,1.0/lr)), 2 * lr * sin(omega / 2.0)));
    if(mystate == ATTRACTION)
        {
            if(lr > COLLISION_R * (cbrt(stars[0].m) + cbrt(stars[satstate].m)) + dt)
                {
                    stars[0].r = add_vel(stars[0].r , multi_vel(r,dt/lr));
                }
        }
    else if(mystate == REPULSION)
        {
            stars[0].r = add_vel(stars[0].r , multi_vel(r, -1 * dt/lr));
        }
    
    //速度の回転
    stars[0].v = multi_vel(rotate_vel(omega, n), v);
    

}


void check_inside(void)
{
    //starsの中にいる星が、内部にいるかどうかcheckし、いなかったら反対側に出す
    for(int i = 0; i < nstars; i++)
        {
            if(stars[i].r.x > SPACE_WIDTH) stars[i].r.x -= 2*SPACE_WIDTH;
            if(stars[i].r.x < -SPACE_WIDTH) stars[i].r.x += 2*SPACE_WIDTH;
            
            if(stars[i].r.y > SPACE_HEIGHT) stars[i].r.y -= 2*SPACE_HEIGHT;
            if(stars[i].r.y < -SPACE_HEIGHT) stars[i].r.y += 2*SPACE_HEIGHT;
        }
    
}

void unify(int i,int j)
{
    //jがiに融合する(iが残る)
    stars[i].v.x = (stars[i].m * stars[i].v.x + stars[j].m * stars[j].v.x)/ (stars[i].m + stars[j].m);
    stars[i].v.y = (stars[i].m * stars[i].v.y + stars[j].m * stars[j].v.y)/ (stars[i].m + stars[j].m);
    stars[i].m += stars[j].m;
    
}

void crash(void)
{
    char space[WIDTH][HEIGHT];
    FILE* fp = stack_fp(NULL);
    for(int i = 0; i < WIDTH;i++)fprintf(fp, "-");
    fprintf(fp,"\n");
    for(int i = 0; i < 8; i++) //ぱかぱかして衝突っぽく演出
        {
            if(i % 2 == 0)
                memset(space,'#', sizeof(space));
            else
                 memset(space,' ', sizeof(space));
            int x = 0,y = 0;
            for (y = 0; y < HEIGHT; y++) {
                for (x = 0; x < WIDTH; x++)
                    fputc(space[x][y], fp);
                fputc('\n', fp);
            }
            fflush(fp);
            usleep(300000);
        }
    
    game_over();
}
void print_opening(FILE * wfp)
{
    FILE *rfp = fopen("rsc/Opening.txt", "r");
    if(rfp == NULL)
        {
            fprintf(stderr,"error: Opening.txt doesn't exist.\n");
            return;
        }
    
    fprintf(wfp,"\n");
    char c[256];
    while(fgets(c,256,rfp) != NULL)
        {
            fprintf(wfp,"%s",c);
        }
    fflush(wfp);
}

int is_goal(void) //ゲームの終了条件
{
    if(nstars == 1) return 1;
    else if(stars[0].m >= GOAL_M) return 1;
    else return 0;
}

void game_over(void)
{
    printf("Your planet has be broken. Game Over\n");
    printf("note: R-planet is bigger than you. It may break your planet into pieces.\n");
    exit(1);
}

int main(int argc, char **argv)
{
  const char *filename = "space.txt";
  FILE *fp;
  if ((fp = fopen(filename, "a")) == NULL) {
    fprintf(stderr, "error: cannot open %s.\n", filename);
    return 1;
  }

  stack_fp(fp);
  //ステージの読み込み
  FILE *rfp = fopen("rsc/planets.txt","r");
  if(rfp == NULL) 
      {
          fprintf(stderr, "error: cannot open planets.txt.\n");
          return 1;
      }
  int maxlen = 10;
  nstars = 0;
  stars = malloc(sizeof(STAR) * maxlen);
  memset(stars, 0, sizeof(STAR) * maxlen);
  STAR tmp = {};
  while(fscanf(rfp,"%lf %lf %lf %lf %lf %*[\n]",&(tmp.m),&(tmp.r.x), &(tmp.r.y), &(tmp.v.x), &(tmp.v.y)) != EOF)
      {
          if(nstars >= maxlen)
              {
                  maxlen += 10;
                  stars = realloc(stars,sizeof(STAR) * maxlen);
              }
          stars[nstars] = tmp;
          nstars++;
      }
  fclose(rfp);
  printf("Welcome to Orbital!\n");
  printf("How to play : a for attraction s for repulsion other keys for normal state\n");
  printf("Goal: To make your planet(@) bigger. Connect B-planets, whose mass is equal to you.\n");
  printf("\n");
  printf("Don't connect your planet with R-planets, which will break your planet into pieces.\n");
  printf("But You can lift your planets on R's satellite orbit. Try crossing near R-planets.\n");
  
  printf("\nENJOY!\n");
  

  print_opening(fp);
  getchar();
  
  int fl_rate = 100;
  if(argc != 1)
      {
          fl_rate = atoi(argv[1]);
      }
  
  const double stop_time = 400;

  int i;
  double t;
  const double dt = 1.0/(double) fl_rate;
  for (i = 0, t = 0; t <= stop_time; i++, t += dt) {
      check_satstate(dt);
      if(satstate == FREE){
          runge_kutta(dt);
      }else
          {
              on_the_satellite_orbit(dt);
          }
      move_other_stars(dt);
      check_inside();
      detect_collision();
    
      if(i % (fl_rate / 10) == 0) 
          {
          plot_stars(fp);
          if(is_goal() == 1)
              {
                  printf("GOAL!!!\n");
                  printf("You'd succeeded to make your planet bigger.\n");
                  printf("Clear time :%f\n",t);
                  break;
              }
          
          }
      
    if(i % fl_rate == 0) plot_status(t);
    
    if(kbhit())
        {
            char c = getch();
            switch(c)
                {
                case 'a':
                    mystate = ATTRACTION;
                    break;
                case 's':
                    mystate = REPULSION;
                    break;
                default:
                    mystate = NORMAL;
                    break;
                }
            
        }
    
    usleep(1000000 / fl_rate );
  }

  printf("Thanks for playing this game!\n");
  printf("See you!\n");
  
  fclose(fp);

  return 0;
}
