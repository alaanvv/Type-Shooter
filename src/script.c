// TODO
// Different enemies
// Crosshair
// Add songs

#include "canvas.h"
#include <string.h>

#define IS_UPPER(c) (c >= 'A' && c <= 'Z')
#define TO_UPPER(c) { c += 'A' - 'a'; }
#define TO_LOWER(c) { c += 'a' - 'A'; }
#define LOWER(c)    ( c +  'a' - 'A'  )

#define UPSCALE 1

#define WORDS_PATH "../words.txt"
#define MAX_WORDS  1000

#define MAX_OFFSET 0.6 * MAX_COLS
#define ROW_HEIGHT 4
#define ROW_SPACE  -10
#define DISTANCE   20
#define SPEED      0.05
#define GAP        4

#define MAX_ROWS 4
#define MAX_COLS 5

// ---

typedef enum {
  START_SCREEN, MODE_SCREEN, GAME_SCREEN, END_SCREEN
} Stage;

typedef struct {
  char words[1000][32];
  u16 size;
} WordPack;

typedef struct {
  vec3 pos;
  u8 active;
  f32 rotation;
} Bullet;

typedef struct {
  char word[32];
  vec3 pos;
  u8 active;
  f32 prog, rotation;
} EnemyBullet;

typedef struct {
  EnemyBullet bullet;
  char word[32];
  f32 prog, next_shoot;
  vec3 pos;
} Enemy;

// ---

void init();
void move_enemy(u8 i, u8 j);
void enemy_shoot(u8 i);
void move_enemy_bullet(u8 i);
void move_player_to_enemy_bullet(u8 i);
void move_player_to_bullet_bullet(u8 i);
void draw_game();
f32  x_angle(vec3 from, vec3 to);
void move_to(vec3 from, vec3 to, f32 speed);
void init_enemies();
void move_rows();
void stage_start();
void draw_start();
void draw_ship();
void draw_stars();
void stage_game();
void stage_mode();
void draw_mode();
void char_press(GLFWwindow* window, u32 key);
void draw_bullet(Model* model, vec3 pos, f32 rotation);
void draw_text(char* word, vec3 pos, Font font, Material mat, f32 size, vec3 typed_color, f32 prog);
int  should_stage_in_event();
void set_stage(Stage stage);

// ---

Enemy  enemies[MAX_ROWS][MAX_COLS];
Bullet bullets[2][MAX_COLS];
vec3   stars[200];
WordPack wp = { 0 };
Stage stage;
u8 stage_in_event;

f32 recoil = 0, blink = 0, x_offset = 0, enemy_bullet_speed;
u8 rows = MAX_ROWS, cols = MAX_COLS, difficulty = 0;
i8 dir = 1;

char menu_play[32];
char menu_easy[32];
char menu_normal[32];
char menu_hardcore[32];
f32  menu_prog = 0;
f32  menu_e    = 0;
f32  menu_n    = 0;
f32  menu_hc   = 0;

// ---

Camera cam = { PI4, 0.01, 100 };
u32  shader;
f32  fps;

Material m_ship     = { GRAY,          0.5, 0.5 };
Material m_enemy    = { DEEP_GREEN,    0.5, 0.5 };
Material m_bullet   = { PURPLE,        0.5, 0.5 };
Material m_bullet_e = { PASTEL_YELLOW, 0.5, 0.5 };
Material m_bullet_d = { DEEP_RED,      0.5, 0.5 };
Material m_star     = { WHITE,         .lig = 1 };
Material m_text     = { PASTEL_GREEN,  .lig = 1 };
Material m_text_w   = { PURPLE,        .lig = 1 };
DirLig light        = { WHITE,    { 0, 0 , -1 } };
Font font           = { GL_TEXTURE0, 60, 30, 5, 7.0 / 5 };
Model* ship, * enemy, * bullet, * star;
u32 lowres_fbo;

// ---

int main() {
  init();

  while (!glfwWindowShouldClose(cam.window)) {
    if      (stage == START_SCREEN) stage_start();
    else if (stage ==  MODE_SCREEN) stage_mode();
    else if (stage ==  GAME_SCREEN) stage_game();

    glfwSwapBuffers(cam.window);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwPollEvents();
    update_fps(&fps);
    if (glfwGetKey(cam.window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(cam.window, 1);
  }

  glfwTerminate();
  return 0;
}

// ---

void init() {
  canvas_init(&cam, (CanvasInitConfig) { "Type Shooter", 1, 0, 0.8, { 0.05, 0, 0.05 } });
  glfwSetCharCallback(cam.window, char_press);
  srand((uintptr_t) &cam);

  ship   = model_create("obj/ship.obj",  &m_ship,   1);
  enemy  = model_create("obj/enemy.obj", &m_enemy,  1);
  bullet = model_create("obj/cube.obj",  &m_bullet, 1);
  star   = model_create("obj/star.obj",  &m_star,   1);

  canvas_create_texture(GL_TEXTURE0, "img/font.ppm",  TEXTURE_DEFAULT);

  shader = shader_create_program("shd/obj.v", "shd/obj.f");
  generate_proj_mat(&cam, shader);
  generate_view_mat(&cam, shader);
  canvas_set_dir_lig(shader, light, 0);

  lowres_fbo = canvas_create_FBO(cam.width * UPSCALE, cam.height * UPSCALE, GL_NEAREST, GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // ---

  for (u8 i = 0; i < LEN(stars); i++)
    VEC3_COPY(VEC3(RAND(-60, 60), RAND(-60, 60), -RAND(50, 60)), stars[i]);

  FILE *file = fopen(WORDS_PATH, "r");
  ASSERT(file, "Error opening words");
  char buffer[32];
  while (wp.size < LEN(wp.words) && fscanf(file, "%s", buffer) == 1)
    strcpy(wp.words[wp.size++], buffer);
  fclose(file);

  set_stage(START_SCREEN);
}

// ---

void stage_start() {
  if (should_stage_in_event()) {
    strcpy(menu_play, "play");
  }

  if (blink)  blink  = MAX(blink  - 0.100, 0);

  draw_start();
}

void stage_mode() {
  if (should_stage_in_event()) {
    strcpy(menu_easy, "easy");
    strcpy(menu_normal, "normal");
    strcpy(menu_hardcore, "hardcore");
  }

  if (blink)  blink  = MAX(blink  - 0.100, 0);

  draw_mode();
}

void stage_game() {
  if (should_stage_in_event()) {
    difficulty++;
    rows = MIN(MAX_ROWS, 1 + difficulty);
    cols = MIN(MAX_COLS, MAX(2, difficulty));
    enemy_bullet_speed = 0.04 + (MIN(10, MAX(1, difficulty - 3)) / 10.0 * 0.05);
    init_enemies();
  }

  // Decrease stuff
  if (recoil) recoil = MAX(recoil - 0.001, 0);
  if (blink)  blink  = MAX(blink  - 0.100, 0);

  if (x_offset >  MAX_OFFSET && dir ==  1) dir = -1;
  if (x_offset < -MAX_OFFSET && dir == -1) dir =  1;
  x_offset += dir * SPEED;

  for (u8 i = 0; i < cols; i++) {
    for (u8 j = 0; j < rows; j++)
      move_enemy(j, i);
    enemy_shoot(i);
    move_enemy_bullet(i);
    move_player_to_enemy_bullet(i);
    move_player_to_bullet_bullet(i);
  }

  draw_game();
}

// ---

void move_enemy(u8 i, u8 j) {
  VEC3_COPY(VEC3((j - (cols - 1) / 2.0) * GAP + x_offset * (i % 2 ? 1 : -1) * (rows % 2 ? 1 : -1), i * ROW_HEIGHT, -DISTANCE + i * ROW_SPACE), enemies[i][j].pos);
}

void enemy_shoot(u8 i) {
  if (enemies[0][i].next_shoot - glfwGetTime() >= 0 || enemies[0][i].bullet.active || !enemies[0][i].word[0]) return;
  enemies[0][i].bullet.prog = 0;
  enemies[0][i].bullet.active = 1;
  VEC3_COPY(enemies[0][i].pos, enemies[0][i].bullet.pos);
  enemies[0][i].bullet.rotation = x_angle(enemies[0][i].bullet.pos, VEC3(0, 0, 0));
  enemies[0][i].next_shoot = glfwGetTime() + MAX(1, MIN(20, RAND(10, 20) - difficulty));
  strcpy(enemies[0][i].bullet.word, wp.words[RAND(0, wp.size)]);
}

void lose() {
  blink = 10;
  set_stage(START_SCREEN);
}

void move_enemy_bullet(u8 i) {
  if (!enemies[0][i].bullet.active) return;
  move_to(enemies[0][i].bullet.pos, VEC3(0.5 * (i-(cols/2.0)), -0.2, 0), enemy_bullet_speed);

  if (enemies[0][i].bullet.pos[2] <  - 1) return;
  enemies[0][i].bullet.active = 0;
  lose();
}

void check_enemies() {
  for (u8 i = 0; i < cols; i++) {
    if (enemies[0][i].word[0]) break;
    if (i != cols - 1) continue;
    move_rows();
  }
}

void move_player_to_enemy_bullet(u8 i) {
  if (!bullets[0][i].active) return;
  move_to(bullets[0][i].pos, enemies[0][i].pos, 0.8);
  bullets[0][i].rotation = x_angle(enemies[0][i].pos, bullets[0][i].pos);

  if (bullets[0][i].pos[2] > -DISTANCE+1) return;
  bullets[0][i].active = 0;
  check_enemies();
}

void move_player_to_bullet_bullet(u8 i) {
  if (!bullets[1][i].active) return;
  move_to(bullets[1][i].pos, enemies[0][i].bullet.pos, 0.8);
  bullets[1][i].rotation = x_angle(enemies[0][i].bullet.pos, bullets[1][i].pos);

  if (bullets[1][i].pos[2] - enemies[0][i].bullet.pos[2] > 0.5) return;
  bullets[1][i].active = 0;
  enemies[0][i].bullet.active = 0;
}

void init_enemies() {
  for (u8 i = 0; i < cols; i++) {
    enemies[0][i].next_shoot = glfwGetTime() + RAND(2, 9) + (5 / (0.1 * 60));
    enemies[0][i].bullet.active = 0;
    enemies[0][i].prog = 0;
    strcpy(enemies[0][i].word, wp.words[RAND(0, wp.size)]);
  }
}

void move_rows() {
  blink = 5;
  rows = rows - 1;
  if (!rows) set_stage(GAME_SCREEN);
  init_enemies();
}

u8 insert_input(char* word, u32 key) {
  for (u8 i = 0; word[i]; i++) {
    if (IS_UPPER(word[i])) continue;

    else if (word[i] == key) {
      if (!word[i + 1]) {
        word[0] = 0;
        return 3;
      }
      TO_UPPER(word[i]);
      return 2;
    }

    else {
      for (i8 j = i - 1; j >= 0; j--)
        if (j != 0 || LOWER(word[j]) != key)
          TO_LOWER(word[j]);
      return 1;
    }
  }
  return 0;
}

void shot(u8 bullet, u8 target) {
  VEC3_COPY(VEC3(0, -3, -5), bullets[bullet][target].pos);
  bullets[bullet][target].active = 1;
  recoil += 0.02;
}

void char_press(GLFWwindow* window, u32 key) {
  if (stage == GAME_SCREEN) {
    for (u8 i = 0; i < cols; i++) {
      u8 r = insert_input(enemies[0][i].word, key);
      if (r == 1) enemies[0][i].prog = 0;
      if (r == 2) enemies[0][i].prog += 0.1;
      if (r == 3) shot(0, i);

      r = insert_input(enemies[0][i].bullet.word, key);
      if (r == 1) enemies[0][i].bullet.prog = 0;
      if (r == 2) enemies[0][i].bullet.prog += 0.1;
      if (r == 3) shot(1, i);
    }
  }
  else if (stage == START_SCREEN) {
    u8 r = insert_input(menu_play, key);
    if (r == 1) menu_prog = 0;
    if (r == 2) menu_prog += 0.1;
    if (r == 3) {
      blink = 5;
      set_stage(MODE_SCREEN);
    }
  }
  else if (stage == MODE_SCREEN) {
    u8 r = insert_input(menu_play, key);
    if (r == 1) menu_prog = 0;
    if (r == 2) menu_prog += 0.1;
    if (r == 3) {
      blink = 5;
      set_stage(GAME_SCREEN);
    }
    r = insert_input(menu_easy, key);
    if (r == 1) menu_e = 0;
    if (r == 2) menu_e += 0.1;
    if (r == 3) {
      blink = 5;
      difficulty = 0;
      set_stage(GAME_SCREEN);
    }
    r = insert_input(menu_normal, key);
    if (r == 1) menu_n = 0;
    if (r == 2) menu_n += 0.1;
    if (r == 3) {
      blink = 5;
      difficulty = 2;
      set_stage(GAME_SCREEN);
    }
    r = insert_input(menu_hardcore, key);
    if (r == 1) menu_hc = 0;
    if (r == 2) menu_hc += 0.1;
    if (r == 3) {
      blink = 5;
      difficulty = 5;
      set_stage(GAME_SCREEN);
    }
  }
}

// ---

void draw_game() {
  if (blink) return;

  draw_ship();
  draw_stars();

  for (u8 i = 0; i < rows; i++)
    for (u8 j = 0; j < cols; j++) {
      if (i || enemies[i][j].word[0] || bullets[i][j].active) {
        model_bind(enemy, shader);
        glm_translate(enemy->model, enemies[i][j].pos);
        model_draw(enemy, shader);
      }

      model_bind(bullet, shader);
      if (bullets[0][j].active)
        draw_bullet(bullet, bullets[0][j].pos, bullets[0][j].rotation);

      if (bullets[1][j].active)
        draw_bullet(bullet, bullets[1][j].pos, bullets[1][j].rotation);

      u8 danger = 0;
      if      (enemies[0][j].bullet.pos[2] >= - 1 -  40 * enemy_bullet_speed) {                                  danger = 1; }
      else if (enemies[0][j].bullet.pos[2] >= - 1 - 120 * enemy_bullet_speed) { if (sin(glfwGetTime() * 30) > 0) danger = 1; }
      else if (enemies[0][j].bullet.pos[2] >= - 1 - 180 * enemy_bullet_speed) { if (sin(glfwGetTime() * 20) > 0) danger = 1; }
      else if (enemies[0][j].bullet.pos[2] >= - 1 - 240 * enemy_bullet_speed) { if (sin(glfwGetTime() * 10) > 0) danger = 1; }
      canvas_set_material(shader, danger ? m_bullet_d : m_bullet_e);

      if (enemies[0][j].bullet.active)
        draw_bullet(bullet, enemies[0][j].bullet.pos, enemies[0][j].bullet.rotation);
    }

  // Lowres
  glBlitNamedFramebuffer(0, lowres_fbo, 0, 0, cam.width, cam.height, 0, 0, cam.width * UPSCALE, cam.height * UPSCALE, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  glBlitNamedFramebuffer(lowres_fbo, 0, 0, 0, cam.width * UPSCALE, cam.height * UPSCALE, 0, 0, cam.width, cam.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

  glClear(GL_DEPTH_BUFFER_BIT);
  for (u8 i = 0; i < cols; i++) {
    draw_text(enemies[0][i].word, VEC3(enemies[0][i].pos[0] - canvas_text_width(enemies[0][i].word, font, 0.01) / 2, 1.3, -DISTANCE), font, m_text, 0.01, (vec3) PURPLE, enemies[0][i].prog);
    if (enemies[0][i].bullet.active)
      draw_text(enemies[0][i].bullet.word, VEC3(enemies[0][i].bullet.pos[0] - canvas_text_width(enemies[0][i].bullet.word, font, 0.01) / 2, enemies[0][i].bullet.pos[1] - 1.3, enemies[0][i].bullet.pos[2]), font, m_text, 0.01, (vec3) PURPLE, enemies[0][i].bullet.prog);
  }
}

void draw_start() {
  vec3 _c;
  VEC3_COPY(m_text.col, _c);
  VEC3_COPY((vec3) PURPLE, m_text.col);
  canvas_draw_text(shader, "type shooter", -canvas_text_width("type shooter", font, 0.05) / 2, 3, -DISTANCE, 0.05, font, m_text, VEC3(0, 0, 0));
  VEC3_ADD(m_text.col, VEC3(-0.3, -0.3, -0.3));
  canvas_draw_text(shader, "type shooter", -canvas_text_width("type shooter", font, 0.05) / 2, 2.9, -DISTANCE - 0.5, 0.05, font, m_text, VEC3(0, 0, 0));
  VEC3_COPY(_c, m_text.col);

  draw_text(menu_play, VEC3(-canvas_text_width(menu_play, font, 0.03) / 2, -2, -DISTANCE), font, m_text, 0.03, (vec3) PURPLE, menu_prog);

  draw_stars();
  draw_ship();
}

void draw_mode() {
  if (blink) return;

  vec3 _c;
  VEC3_COPY(m_text.col, _c);
  VEC3_COPY((vec3) PURPLE, m_text.col);
  canvas_draw_text(shader, "type shooter", -canvas_text_width("type shooter", font, 0.05) / 2, 3, -DISTANCE, 0.05, font, m_text, VEC3(0, 0, 0));
  VEC3_ADD(m_text.col, VEC3(-0.3, -0.3, -0.3));
  canvas_draw_text(shader, "type shooter", -canvas_text_width("type shooter", font, 0.05) / 2, 2.9, -DISTANCE - 0.5, 0.05, font, m_text, VEC3(0, 0, 0));
  VEC3_COPY(_c, m_text.col);

  draw_text(menu_easy,   VEC3(-canvas_text_width(menu_easy,   font, 0.03) / 2, 0, -DISTANCE), font, m_text, 0.03, (vec3) PURPLE, menu_e);
  draw_text(menu_normal,     VEC3(-canvas_text_width(menu_normal,     font, 0.03) / 2, -2.2, -DISTANCE), font, m_text, 0.03, (vec3) PURPLE, menu_n * 2);
  draw_text(menu_hardcore, VEC3(-canvas_text_width(menu_hardcore, font, 0.03) / 2, -4.4, -DISTANCE), font, m_text, 0.03, (vec3) DEEP_RED, menu_hc * 3);

  draw_stars();
  draw_ship();
}

// ---

void draw_ship() {
  model_bind(ship, shader);
  glm_translate(ship->model, VEC3(0, 0, recoil));
  model_draw(ship, shader);
}

void draw_stars() {
  for (u8 i = 0; i < LEN(stars); i++) {
    model_bind(star, shader);
    glm_translate(star->model, VEC3(stars[i][0], stars[i][1], stars[i][2]));
    model_draw(star, shader);
  }
}

void draw_bullet(Model* model, vec3 pos, f32 rotation) {
  glm_mat4_identity(model->model);
  glm_translate(model->model, pos);
  glm_rotate(model->model, rotation, VEC3(0, 1, 0));
  glm_rotate(model->model, PI4/8, VEC3(1, 0, 0));
  glm_scale(model->model, VEC3(0.5, 0.5, 1));
  model_draw(model, shader);
}

void draw_text(char* word, vec3 pos, Font font, Material mat, f32 size, vec3 typed_color, f32 prog) {
  pos[0] += sin((glfwGetTime()) * (1 + prog) * 10) * prog * 0.10;
  pos[1] += cos((glfwGetTime()) * (2 + prog) *  8) * prog * 0.13;

  c8 typed[32];
  u8 j = 0;
  for (; IS_UPPER(word[j]); j++) typed[j] = word[j];
  typed[j] = 0;

  vec3 _c;
  VEC3_COPY(mat.col, _c);
  canvas_draw_text(shader, &word[j], pos[0] + canvas_text_width(typed, font, size), pos[1], pos[2], size, font, mat, VEC3(0, 0, 0));
  VEC3_COPY(typed_color, mat.col);
  canvas_draw_text(shader, typed,    pos[0],                                        pos[1], pos[2], size, font, mat, VEC3(0, 0, 0));
  VEC3_COPY(_c, mat.col);
}

// ---

int should_stage_in_event() {
  int tmp = stage_in_event;
  stage_in_event = 1;
  return !tmp;
}

void set_stage(Stage _stage) {
  stage = _stage;
  stage_in_event = 0;
}

// ---

f32 x_angle(vec3 from, vec3 to) {
  return atan2f(to[0] - from[0], to[2] - from[2]);
}

void move_to(vec3 from, vec3 to, f32 speed) {
  vec3 dir;
  glm_vec3_sub(to, from, dir);
  glm_normalize(dir);
  glm_vec3_scale(dir, speed, dir);
  glm_vec3_add(from, dir, from);
}
