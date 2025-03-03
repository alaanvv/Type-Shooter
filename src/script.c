#define MINIAUDIO_IMPLEMENTATION
#define MAX_SOUNDS 10
#include "miniaudio.h"
#include "canvas.h"
#include <string.h>

#define IS_UPPER(c) (c >= 'A' && c <= 'Z')
#define TO_UPPER(c) { c += 'A' - 'a'; }
#define TO_LOWER(c) { c += 'a' - 'A'; }
#define LOWER(c)    ( c +  'a' - 'A'  )

#define BIG_WORDS_PATH "big_words.txt"
#define WORDS_PATH     "words.txt"
#define MAX_WORDS      1000

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
  START_SCREEN, MODE_SCREEN, GAME_SCREEN
} Stage;

typedef struct {
  int running;
  float start, duration;
} Animation;

typedef struct {
  char words[1000][32];
  u16 size;
} WordPack;

typedef struct {
  char w[32];
  f32 prog;
} TypableWord;

typedef struct {
  vec3 pos;
  u8 active;
  f32 rotation;
} Bullet;

typedef struct {
  TypableWord word;
  vec3 pos;
  u8 active;
  f32 rotation;
} EnemyBullet;

typedef struct {
  EnemyBullet bullet;
  TypableWord word;
  f32 next_shoot;
  vec3 pos;
} Enemy;

typedef struct {
  TypableWord word;
  f32 next_spawn, abducting;
  vec3 pos;
  u8 stage;
} UFO;

// ---

void init();
void stage_start();
void stage_mode();
void stage_game();
void move_ufo();
void move_enemy(u8, u8);
void enemy_shoot(u8);
void move_enemy_bullet(u8);
void move_player_to_enemy_bullet(u8);
void move_player_to_bullet_bullet(u8);
void init_enemies();
void move_rows();
void char_press(GLFWwindow*, u32);
void draw_game();
void draw_start();
void draw_mode();
void draw_ship();
void draw_stars();
void draw_bullet(Model*, vec3, f32);
void draw_text(char*, vec3, Font, Material, f32, vec3, f32);
int  should_stage_in_event();
void set_stage(Stage);
void start_transition(Stage);
void draw_transition();
f32  x_angle(vec3, vec3);
void move_to(vec3, vec3, f32);
void play_audio(char*);
void play_audio_loop(char*);
void stop_audio(char*);
void preload_audio(char**, u8);

// ---

TypableWord play, normal, hardcore;
Enemy  enemies[MAX_ROWS][MAX_COLS];
Bullet bullets[2][MAX_COLS];
WordPack wp = { 0 }, bwp = { 0 };
vec3 stars[200];
Stage stage;
u8 stage_in_event;
UFO ufo;

f32 recoil, blink, x_offset, enemy_bullet_speed;
u8 rows = MAX_ROWS, cols = MAX_COLS, difficulty = 0;
u8 moving = 0;
i8 dir = 1;

// ---

Camera cam = { PI4, 0.01, 100 };
u32  shader, hud_shader;
f32  fps;
Animation transition = { 0, 0, 0.5 };
Stage transition_to;
u8 transitioned = 0;

ma_engine engine;
ma_sound sounds[MAX_SOUNDS];
c8* sound_names[MAX_SOUNDS];
u8 sound_count = 0;

Material m_ship     = { GRAY,          0.5, 0.5 };
Material m_enemy    = { DEEP_GREEN,    0.5, 0.5 };
Material m_ufo      = { DEEP_RED,      0.5, 0.5 };
Material m_bullet   = { PURPLE,        0.5, 0.5 };
Material m_bullet_e = { PASTEL_YELLOW, 0.5, 0.5 };
Material m_bullet_d = { DEEP_RED,      0.5, 0.5 };
Material m_star     = { WHITE,         .lig = 1 };
Material m_text     = { PASTEL_GREEN,  .lig = 1 };
Material m_text_w   = { PURPLE,        .lig = 1 };
DirLig light        = { WHITE,    { 0, 0 , -1 } };
Font font           = { GL_TEXTURE0, 60, 30, 5, 7.0 / 5 };
Model* ship, * enemy, * enemy_2, * bullet, * star, * model_ufo;
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
  ASSERT(ma_engine_init(NULL, &engine) == MA_SUCCESS, "Failed to init audio");
  char* audios[] = { "abduction", "ufo", "key", "enemy", "death", "enter", "miss", "next", "shoot" };
  preload_audio(audios, LEN(audios));

  canvas_init(&cam, (CanvasInitConfig) { "Type Shooter", 1, 0, 0.8, { 0.05, 0, 0.05 } });
  glfwSetCharCallback(cam.window, char_press);
  srand((uintptr_t) &cam);

  ship      = model_create("obj/ship.obj",    &m_ship,   1);
  enemy     = model_create("obj/enemy.obj",   &m_enemy,  1);
  enemy_2   = model_create("obj/enemy_2.obj", &m_enemy,  1);
  bullet    = model_create("obj/cube.obj",    &m_bullet, 1);
  star      = model_create("obj/star.obj",    &m_star,   1);
  model_ufo = model_create("obj/ufo.obj",     &m_ufo,   1);

  canvas_create_texture(GL_TEXTURE0, "img/font.ppm",  TEXTURE_DEFAULT);

  hud_shader = shader_create_program("shd/hud.v", "shd/hud.f");
  generate_ortho_mat(&cam, hud_shader);

  shader = shader_create_program("shd/obj.v", "shd/obj.f");
  generate_proj_mat(&cam, shader);
  generate_view_mat(&cam, shader);
  canvas_set_dir_lig(shader, light, 0);

  // ---

  for (u8 i = 0; i < LEN(stars); i++)
    VEC3_COPY(VEC3(RAND(-60, 60), RAND(-60, 60), -RAND(50, 60)), stars[i]);

  FILE *file = fopen(WORDS_PATH, "r");
  ASSERT(file, "Error opening words");
  c8 buffer[32];
  while (wp.size < LEN(wp.words) && fscanf(file, "%s", buffer) == 1)
    strcpy(wp.words[wp.size++], buffer);
  fclose(file);

  file = fopen(BIG_WORDS_PATH, "r");
  ASSERT(file, "Error opening big words");
  while (bwp.size < LEN(bwp.words) && fscanf(file, "%s", buffer) == 1)
    strcpy(bwp.words[bwp.size++], buffer);
  fclose(file);

  set_stage(START_SCREEN);
}

// ---

void stage_start() {
  if (should_stage_in_event()) {
    strcpy(play.w, "play");
    play.prog = 0;
  }

  draw_start();
}

void stage_mode() {
  if (should_stage_in_event()) {
    strcpy(normal.w, "normal");
    normal.prog = 0;
    strcpy(hardcore.w, "hardcore");
    hardcore.prog = 0;
  }

  draw_mode();
}

void stage_game() {
  if (should_stage_in_event()) {
    difficulty++;
    rows = MIN(MAX_ROWS, 1 + difficulty);
    cols = MIN(MAX_COLS, MAX(1, difficulty));
    enemy_bullet_speed = 0.04 + (MIN(10, MAX(1, difficulty - 3))) / 10.0 * 0.1;
    moving = 0;
    ufo.next_spawn = glfwGetTime() + RAND(5, 10);
    ufo.stage = 0;
    recoil = 0;
    x_offset = 0;
    init_enemies();
  }

  // Decrease stuff
  if (recoil)    recoil    = MAX(recoil    - 0.001 * (60 / fps), 0);
  if (blink)     blink     = MAX(blink     - 0.100 * (60 / fps), 0);
  if (ufo.abducting) ufo.abducting = MAX(ufo.abducting - 0.100 * (60 / fps), 0);

  if (x_offset >  MAX_OFFSET && dir ==  1) dir = -1;
  if (x_offset < -MAX_OFFSET && dir == -1) dir =  1;
  x_offset += dir * SPEED * (60 / fps);

  move_ufo();
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

void move_ufo() {
  if (ufo.stage == 0 && glfwGetTime() > ufo.next_spawn && !blink) {
    play_audio_loop("ufo");
    ufo.stage = 1;
    VEC3_COPY(VEC3(0, 10, -DISTANCE), ufo.pos);
    strcpy(ufo.word.w, bwp.words[RAND(0, bwp.size)]);
    ufo.word.prog = 0;
  }

  else if (ufo.stage == 1) {
    ufo.pos[1] -= 0.05;
    if (ufo.pos[1] <= 6) ufo.stage++;
  }

  else if (ufo.stage > 1 && ufo.stage < 5) {
    ufo.pos[0] += 0.02 * (ufo.stage % 2 ? 1 : -1);
    if (ufo.pos[0] < -MAX_OFFSET*2 || ufo.pos[0] > MAX_OFFSET*2) ufo.stage++;
  }

  else if (ufo.stage == 7) {
    stop_audio("ufo");
    ufo.abducting = 10;
    ufo.pos[1] += 10;
    ufo.stage = 8;
    play_audio("abduction");
    for (u8 i = 0; i < cols; i++) {
      enemies[0][i].next_shoot = glfwGetTime() + RAND(2, 9) + (15 / (0.1 * 60));
      enemies[0][i].bullet.active = 0;
      enemies[0][i].word.w[1] = 0;
      if (IS_UPPER(enemies[0][i].word.w[0])) TO_LOWER(enemies[0][i].word.w[0]);
    }
  }
}

void move_enemy(u8 i, u8 j) {
  VEC3_COPY(VEC3((j - (cols - 1) / 2.0) * GAP + x_offset * (i % 2 ? 1 : -1) * (rows % 2 ? 1 : -1), i * ROW_HEIGHT, -DISTANCE + i * ROW_SPACE), enemies[i][j].pos);
}

void enemy_shoot(u8 i) {
  if (enemies[0][i].next_shoot - glfwGetTime() >= 0 || enemies[0][i].bullet.active || !enemies[0][i].word.w[0]) return;
  enemies[0][i].bullet.word.prog = 0;
  enemies[0][i].bullet.active = 1;
  VEC3_COPY(enemies[0][i].pos, enemies[0][i].bullet.pos);
  enemies[0][i].bullet.rotation = x_angle(enemies[0][i].bullet.pos, VEC3(0, 0, 0));
  enemies[0][i].next_shoot = glfwGetTime() + MAX(1, MIN(20, RAND(10, 20) - difficulty)) + (moving ? 15.0 : 5.0) / 15.0 * 0.05;
  strcpy(enemies[0][i].bullet.word.w, wp.words[RAND(0, wp.size)]);
  play_audio("enemy");
}

void lose() {
  stop_audio("ufo");
  play_audio("death");
  start_transition(START_SCREEN);
}

void move_enemy_bullet(u8 i) {
  if (!enemies[0][i].bullet.active) return;
  move_to(enemies[0][i].bullet.pos, VEC3(0.5 * (i-(cols/2.0)), -0.2, 0), enemy_bullet_speed * (60 / fps));

  if (enemies[0][i].bullet.pos[2] <  - 1) return;
  enemies[0][i].bullet.active = 0;
  lose();
}

void check_enemies() {
  for (u8 i = 0; i < cols; i++) {
    if (enemies[0][i].word.w[0] || bullets[0][i].active) break;
    if (i != cols - 1) continue;
    move_rows();
  }
}

void move_player_to_enemy_bullet(u8 i) {
  if (!bullets[0][i].active) return;
  move_to(bullets[0][i].pos, enemies[0][i].pos, 0.8 * (60 / fps));
  bullets[0][i].rotation = x_angle(enemies[0][i].pos, bullets[0][i].pos);

  if (bullets[0][i].pos[2] > -DISTANCE+1) return;
  bullets[0][i].active = 0;
  check_enemies();
}

void move_player_to_bullet_bullet(u8 i) {
  if (!bullets[1][i].active) return;
  move_to(bullets[1][i].pos, enemies[0][i].bullet.pos, 0.8 * (60 / fps));
  bullets[1][i].rotation = x_angle(enemies[0][i].bullet.pos, bullets[1][i].pos);

  if (bullets[1][i].pos[2] - enemies[0][i].bullet.pos[2] > 0.5) return;
  bullets[1][i].active = 0;
  enemies[0][i].bullet.active = 0;
}

void init_enemies() {
  for (u8 i = 0; i < cols; i++) {
    enemies[0][i].next_shoot = glfwGetTime() + RAND(2, 9) + (15 / (0.1 * 60));
    enemies[0][i].bullet.active = 0;
    enemies[0][i].word.prog = 0;
    strcpy(enemies[0][i].word.w, wp.words[RAND(0, wp.size)]);
  }
}

void move_rows() {
  blink = 5;
  rows = rows - 1;
  if (!rows) {
    set_stage(GAME_SCREEN);
    play_audio("next");
    stop_audio("ufo");
    blink = 15;
    moving = 1;
  }
  init_enemies();
}

u8 insert_input(TypableWord* word, u32 key) {
  for (u8 i = 0; word->w[i]; i++) {
    if (IS_UPPER(word->w[i])) continue;

    else if (word->w[i] == key) {
      play_audio("key");
      if (!word->w[i + 1]) {
        word->w[0] = 0;
        return 3;
      }
      TO_UPPER(word->w[i]);
      word->prog += 0.1;
      return 2;
    }

    else {
      for (i8 j = i - 1; j >= 0; j--)
        if (j != 0 || LOWER(word->w[j]) != key)
          TO_LOWER(word->w[j]);
      if (i > 1) play_audio("miss");
      word->prog = 0;
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
      u8 r = insert_input(&enemies[0][i].word, key);
      if (r == 3) {
        shot(0, i);
        play_audio("shoot");
      }

      r = insert_input(&enemies[0][i].bullet.word, key);
      if (r == 3) {
        shot(1, i);
        play_audio("shoot");
      }
    }
    u8 r = insert_input(&ufo.word, key);
    if (r == 3) ufo.stage = 7;
  }
  else if (stage == START_SCREEN) {
    u8 r = insert_input(&play, key);
    if (r == 3) {
      play_audio("enter");
      start_transition(MODE_SCREEN);
    }
  }
  else if (stage == MODE_SCREEN) {
    u8 r = insert_input(&normal, key);
    if (r == 3) {
      difficulty = 0;
      play_audio("enter");
      start_transition(GAME_SCREEN);
    }
    r = insert_input(&hardcore, key);
    if (r == 3) {
      difficulty = 5;
      play_audio("enter");
      start_transition(GAME_SCREEN);
    }
  }
}

// ---

void draw_game() {
  if (ufo.abducting) {
    glUseProgram(hud_shader);
    hud_draw_rec(hud_shader, 0, (vec3) PASTEL_YELLOW, 0, 0, cam.width, cam.height);
    glUseProgram(shader);
  }
  if (ufo.abducting || blink) return;

  draw_ship();
  draw_stars();

  if (ufo.stage) {
    model_bind(model_ufo, shader);
    glm_translate(model_ufo->model, ufo.pos);
    model_draw(model_ufo, shader);
    if (ufo.stage > 1)
      draw_text(ufo.word.w, VEC3(ufo.pos[0] - canvas_text_width(ufo.word.w, font, 0.01) / 2, ufo.pos[1] + 1.1, ufo.pos[2]), font, m_text, 0.01, (vec3) PURPLE, ufo.word.prog);
  }

  for (u8 i = 0; i < rows; i++)
    for (u8 j = 0; j < cols; j++) {
      if (i || enemies[i][j].word.w[0] || bullets[i][j].active) {
        Model* model = (sin(glfwGetTime() * 3) * ((j + i) % 2 ? -1 : 1)) > 0 ? enemy : enemy_2;
        model_bind(model, shader);
        glm_translate(model->model, enemies[i][j].pos);
        model_draw(model, shader);
      }

      model_bind(bullet, shader);
      if (bullets[0][j].active)
        draw_bullet(bullet, bullets[0][j].pos, bullets[0][j].rotation);

      if (bullets[1][j].active)
        draw_bullet(bullet, bullets[1][j].pos, bullets[1][j].rotation);


      if (enemies[0][j].bullet.active) {
        u8 danger = 0;
        f32 remain = -1 -enemies[0][j].bullet.pos[2];

        if (remain < 1 || (remain < 10 && sin(200 / (fabs(remain) + 1)) > 0)) danger = 1;
        canvas_set_material(shader, danger ? m_bullet_d : m_bullet_e);
        draw_bullet(bullet, enemies[0][j].bullet.pos, enemies[0][j].bullet.rotation);
      }
    }

  glClear(GL_DEPTH_BUFFER_BIT);
  for (u8 i = 0; i < cols; i++) {
    draw_text(enemies[0][i].word.w, VEC3(enemies[0][i].pos[0] - canvas_text_width(enemies[0][i].word.w, font, 0.01) / 2, 1.3, -DISTANCE), font, m_text, 0.01, (vec3) PURPLE, enemies[0][i].word.prog);
    if (enemies[0][i].bullet.active)
      draw_text(enemies[0][i].bullet.word.w, VEC3(enemies[0][i].bullet.pos[0] - canvas_text_width(enemies[0][i].bullet.word.w, font, 0.01) / 2, enemies[0][i].bullet.pos[1] - 1.3, enemies[0][i].bullet.pos[2]), font, m_text, 0.01, (vec3) PURPLE, enemies[0][i].bullet.word.prog);
  }
  draw_transition();
}

void draw_start() {
  vec3 _c;
  VEC3_COPY(m_text.col, _c);
  VEC3_COPY((vec3) PURPLE, m_text.col);
  canvas_draw_text(shader, "type shooter", -canvas_text_width("type shooter", font, 0.05) / 2, 3, -DISTANCE, 0.05, font, m_text, VEC3(0, 0, 0));
  VEC3_ADD(m_text.col, VEC3(-0.3, -0.3, -0.3));
  canvas_draw_text(shader, "type shooter", -canvas_text_width("type shooter", font, 0.05) / 2, 2.9, -DISTANCE - 0.5, 0.05, font, m_text, VEC3(0, 0, 0));
  VEC3_COPY(_c, m_text.col);

  draw_text(play.w, VEC3(-canvas_text_width(play.w, font, 0.03) / 2, -2, -DISTANCE), font, m_text, 0.03, (vec3) PURPLE, play.prog);

  draw_stars();
  draw_ship();
  draw_transition();
}

void draw_mode() {
  vec3 _c;
  VEC3_COPY(m_text.col, _c);
  VEC3_COPY((vec3) PURPLE, m_text.col);
  canvas_draw_text(shader, "type shooter", -canvas_text_width("type shooter", font, 0.05) / 2, 3, -DISTANCE, 0.05, font, m_text, VEC3(0, 0, 0));
  VEC3_ADD(m_text.col, VEC3(-0.3, -0.3, -0.3));
  canvas_draw_text(shader, "type shooter", -canvas_text_width("type shooter", font, 0.05) / 2, 2.9, -DISTANCE - 0.5, 0.05, font, m_text, VEC3(0, 0, 0));
  VEC3_COPY(_c, m_text.col);

  draw_text(normal.w,     VEC3(-canvas_text_width(normal.w,     font, 0.03) / 2, -1, -DISTANCE), font, m_text, 0.03, (vec3) PURPLE, normal.prog * 2);
  draw_text(hardcore.w, VEC3(-canvas_text_width(hardcore.w, font, 0.03) / 2, -4, -DISTANCE), font, m_text, 0.03, (vec3) DEEP_RED, hardcore.prog * 3);

  draw_stars();
  draw_ship();
  draw_transition();
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

void draw_text(c8* word, vec3 pos, Font font, Material mat, f32 size, vec3 typed_color, f32 prog) {
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

void start_animation(Animation* anim) {
  anim->start   = glfwGetTime();
  anim->running = 1;
}

float animation_keyframe(Animation* anim) {
  float x = (glfwGetTime() - anim->start);
  if (x >= anim->duration) anim->running = 0;
  return x;
}

void start_transition(Stage to) {
  start_animation(&transition);
  transition_to = to;
  transitioned = 0;
}

void draw_transition() {
  if (!transition.running) return;
  float x = animation_keyframe(&transition);
  if (x >= transition.duration / 2 && !transitioned) {
    set_stage(transition_to);
    transitioned = 1;
  }

  glUseProgram(hud_shader);
  hud_draw_rec(hud_shader, 0, VEC3(0, 0, 0), -cam.width + x / transition.duration * 2 * cam.width, 0, cam.width, cam.height);
  glUseProgram(shader);
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

// ---

void play_audio(c8* name) {
  for (int i = 0; i < sound_count; i++) {
    if (strcmp(sound_names[i], name) == 0) {
      ma_sound_start(&sounds[i]);
      return;
    }
  }
}

void play_audio_loop(c8* name) {
  for (int i = 0; i < sound_count; i++) {
    if (strcmp(sound_names[i], name) == 0) {
      ma_sound_set_looping(&sounds[i], 1);
      ma_sound_set_volume(&sounds[i], 0.1);
      ma_sound_start(&sounds[i]);
      return;
    }
  }
}

void stop_audio(c8* name) {
  for (int i = 0; i < sound_count; i++) {
    if (strcmp(sound_names[i], name) == 0) {
      ma_sound_stop(&sounds[i]);
      return;
    }
  }
}

void preload_audio(c8** names, u8 amount) {
  for (u8 i = 0; i < amount; i++) {
    ASSERT(sound_count < MAX_SOUNDS, "Using more sounds than max");
    c8 buffer[64] = { 0 };
    sprintf(buffer, "wav/%s.wav", names[i]);
    ASSERT(ma_sound_init_from_file(&engine, buffer, 0, NULL, NULL, &sounds[sound_count]) == MA_SUCCESS, "Failed to load %s", names[i]);
    sound_names[sound_count] = strdup(names[i]);
    sound_count++;
  }
}
