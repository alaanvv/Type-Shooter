#include "canvas.h"

#define IS_UPPER(c) (c >= 'A' && c <= 'Z')
#define TO_UPPER(c) { c += 'A' - 'a'; }
#define TO_LOWER(c) { c += 'a' - 'A'; }
#define LOWER(c)    ( c +  'a' - 'A'  )

#define UPSCALE 1

#define ENEMY_ROWS  5
#define ENEMY_COLS  5
#define STAR_AMOUNT 200

#define MAX_OFFSET       0.6 * ENEMY_COLS
#define ENEMIES_DISTANCE 20
#define ENEMY_SPACE      4
#define ENEMY_SPEED      0.05
#define ROW_SPACE        -10
#define ROW_HEIGHT       2

// ---

Camera cam = { PI4, 0.01, STAR_AMOUNT, .pos = { 0, 0, ENEMIES_DISTANCE } };
u32  shader, hud_shader;
f32  fps;

// ---

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

vec3 stars[STAR_AMOUNT];

Enemy enemies[ENEMY_ROWS][ENEMY_COLS] = {{
  { { "bullet" }, "hello"  },
    { { "kill"   }, "potato" },
    { { "die"    }, "engine" },
    { { "bad"    }, "cats"   },
    { { "evil"   }, "here"   }
}};

Bullet bullets[2][ENEMY_COLS];
f32 recoil = 0;

f32 x_offset = 0;
i8 dir = 1;

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

void init_game() {
  srand((uintptr_t) &cam);
  for (u8 i = 0; i < STAR_AMOUNT; i++)
    VEC3_COPY(VEC3(RAND(-60, 60), RAND(-60, 60), -RAND(50, 60)), stars[i]);

  for (u8 i = 0; i < ENEMY_ROWS; i++)
    enemies[0][i].next_shoot = glfwGetTime() + RAND(3, 10);
}

void game_tick() {
  // Move enemies
  if (x_offset >  MAX_OFFSET && dir ==  1) dir = -1;
  if (x_offset < -MAX_OFFSET && dir == -1) dir =  1;
  x_offset += dir * ENEMY_SPEED;
  for (u8 i = 0; i < ENEMY_ROWS; i++)
    for (u8 j = 0; j < ENEMY_COLS; j++)
      VEC3_COPY(VEC3((j - (ENEMY_COLS - 1) / 2.0) * ENEMY_SPACE + x_offset * (i % 2 ? 1 : -1), i * ROW_HEIGHT, i * ROW_SPACE), enemies[i][j].pos);

  // Recoil
  if (recoil) recoil = MAX(recoil - 0.001, 0);

  for (u8 i = 0; i < ENEMY_COLS; i++) {
    // Enemy shoot
    if (enemies[0][i].next_shoot - glfwGetTime() < 0 && !enemies[0][i].bullet.active && enemies[0][i].word[0]) {
      enemies[0][i].bullet.active = 1;
      enemies[0][i].next_shoot = glfwGetTime() + RAND(20, 40);
      VEC3_COPY(enemies[0][i].pos, enemies[0][i].bullet.pos);
      enemies[0][i].bullet.rotation = x_angle(enemies[0][i].bullet.pos, VEC3(0, 0, ENEMIES_DISTANCE));
    }

    // Move enemy bullet
    if (enemies[0][i].bullet.active) {
      move_to(enemies[0][i].bullet.pos, VEC3(0.5 * (i-(ENEMY_COLS/2.0)), -1, ENEMIES_DISTANCE), 0.05);

      // Lose
      if (enemies[0][i].bullet.pos[2] >= ENEMIES_DISTANCE) {
        enemies[0][i].bullet.active = 0;
        glfwSetWindowShouldClose(cam.window, 1);
      }
    }

    // Move player-to-enemy bullet
    if (bullets[0][i].active) {
      move_to(bullets[0][i].pos, enemies[0][i].pos, 0.8);
      bullets[0][i].rotation = x_angle(enemies[0][i].pos, bullets[0][i].pos);
      if (bullets[0][i].pos[2] <= 0) bullets[0][i].active = 0;
    }

    // Move player-to-bullet bullet
    if (bullets[1][i].active) {
      move_to(bullets[1][i].pos, enemies[0][i].bullet.pos, 0.8);
      bullets[1][i].rotation = x_angle(enemies[0][i].bullet.pos, bullets[1][i].pos);
      if (bullets[1][i].pos[2] - enemies[0][i].bullet.pos[2] <= 0) {
        bullets[1][i].active = 0;
        enemies[0][i].bullet.active = 0;
      }
    }
  }
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
  VEC3_COPY(VEC3(0, -3, 15), bullets[bullet][target].pos);
  bullets[bullet][target].active = 1;
  recoil += 0.02;
}

void char_press(GLFWwindow* window, u32 key) {
  for (u8 i = 0; i < ENEMY_COLS; i++) {
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

// ---

void draw_bullet(u32 shader, Model* model, vec3 pos, f32 rotation) {
  glm_mat4_identity(model->model);
  glm_translate(model->model, pos);
  glm_rotate(model->model, rotation, VEC3(0, 1, 0));
  glm_rotate(model->model, PI4/8, VEC3(1, 0, 0));
  glm_scale(model->model, VEC3(0.5, 0.5, 1));
  model_draw(model, shader);
}

void draw_text(char* word, vec3 pos, Font font, Material mat, f32 size, vec3 typed_color, f32 prog) {
  pos[0] += sin(glfwGetTime() * (1 + prog) * 10) * prog * 0.10;
  pos[1] += cos(glfwGetTime() * (2 + prog) *  8) * prog * 0.13;

  c8 typed[32];
  u8 j = 0;
  for (; IS_UPPER(word[j]); j++) typed[j] = word[j];
  typed[j] = 0;

  vec3 _c;
  VEC3_COPY(mat.col, _c);
  canvas_draw_text(shader, &word[j], pos[0] + canvas_text_width(typed, font, 0.01), pos[1], pos[2], size, font, mat, VEC3(0, 0, 0));
  VEC3_COPY(typed_color, mat.col);
  canvas_draw_text(shader, typed,    pos[0],                                        pos[1], pos[2], size, font, mat, VEC3(0, 0, 0));
  VEC3_COPY(_c, mat.col);
}

int main() {
  init_game();
  canvas_init(&cam, (CanvasInitConfig) { "Type Shooter", 1, 0, 0.8, { 0.05, 0, 0.05 } });
  glfwSetCharCallback(cam.window, char_press);

  // Material
  Material m_ship   = { GRAY,         0.5, 0.5 };
  Material m_enemy  = { DEEP_GREEN,   0.5, 0.5 };
  Material m_bullet = { PURPLE,       0.5, 0.5 };
  Material m_star   = { WHITE,        .lig = 1 };
  Material m_text   = { PASTEL_GREEN, .lig = 1 };
  Material m_text_w = { PURPLE,       .lig = 1 };

  // Light
  DirLig light = { WHITE, { 0, 0 , -1 } };

  // Model
  Model* ship   = model_create("obj/ship.obj",  &m_ship,   1);
  Model* enemy  = model_create("obj/enemy.obj", &m_enemy,  1);
  Model* bullet = model_create("obj/cube.obj",  &m_bullet, 1);
  Model* star   = model_create("obj/star.obj",  &m_star,   1);

  // Texture
  canvas_create_texture(GL_TEXTURE0, "img/font.ppm",  TEXTURE_DEFAULT);

  // Font
  Font font = { GL_TEXTURE0, 60, 30, 5, 7.0 / 5 };

  // Shader
  shader = shader_create_program("shd/obj.v", "shd/obj.f");
  generate_proj_mat(&cam, shader);
  generate_view_mat(&cam, shader);
  canvas_set_dir_lig(shader, light, 0);

  hud_shader = shader_create_program("shd/hud.v", "shd/hud.f");
  generate_ortho_mat(&cam, hud_shader);

  // FBO
  u32 lowres_fbo = canvas_create_FBO(cam.width * UPSCALE, cam.height * UPSCALE, GL_NEAREST, GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  while (!glfwWindowShouldClose(cam.window)) {
    game_tick();

    glUseProgram(shader);

    // Draw Ship
    model_bind(ship, shader);
    glm_translate(ship->model, VEC3(0, 0, ENEMIES_DISTANCE + recoil));
    model_draw(ship, shader);

    for (u8 i = 0; i < ENEMY_ROWS; i++)
      for (u8 j = 0; j < ENEMY_COLS; j++) {
        // Skip dead enemies
        if (!i && !enemies[i][j].word[0] && !bullets[i][j].active) continue;

        // Draw enemy
        model_bind(enemy, shader);
        glm_translate(enemy->model, enemies[i][j].pos);
        model_draw(enemy, shader);

        model_bind(bullet, shader);
        // Draw player's bullet to enemy
        if (bullets[0][j].active)
          draw_bullet(shader, bullet, bullets[0][j].pos, bullets[0][j].rotation);

        // Draw player's bullet to bullet
        if (bullets[1][j].active)
          draw_bullet(shader, bullet, bullets[1][j].pos, bullets[1][j].rotation);

        // Draw enemy bullet
        canvas_uni3f(shader, "MAT.COL", (vec3) PASTEL_YELLOW[0], (vec3) PASTEL_YELLOW[1], (vec3) PASTEL_YELLOW[2]);
        if (enemies[0][j].bullet.active)
          draw_bullet(shader, bullet, enemies[0][j].bullet.pos, enemies[0][j].bullet.rotation);
      }

    for (u8 i = 0; i < STAR_AMOUNT; i++) {
      model_bind(star, shader);
      glm_translate(star->model, VEC3(stars[i][0], stars[i][1], stars[i][2]));
      model_draw(star, shader);
    }

    // Lowres
    glBlitNamedFramebuffer(0, lowres_fbo, 0, 0, cam.width, cam.height, 0, 0, cam.width * UPSCALE, cam.height * UPSCALE, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitNamedFramebuffer(lowres_fbo, 0, 0, 0, cam.width * UPSCALE, cam.height * UPSCALE, 0, 0, cam.width, cam.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    for (u8 i = 0; i < ENEMY_COLS; i++) {
      f32 x = enemies[0][i].pos[0] - canvas_text_width(enemies[0][i].word, font, 0.01) / 2;
      draw_text(enemies[0][i].word, VEC3(x, -1.3, 0), font, m_text, 0.01, (vec3) PURPLE, enemies[0][i].prog);

      if (!enemies[0][i].bullet.active) continue;
      x = enemies[0][i].bullet.pos[0] - canvas_text_width(enemies[0][i].bullet.word, font, 0.01) / 2;
      f32 y = enemies[0][i].bullet.pos[1]-1.3;
      f32 z = enemies[0][i].bullet.pos[2];
      draw_text(enemies[0][i].bullet.word, VEC3(x, y, z), font, m_text, 0.01, (vec3) PURPLE, enemies[0][i].bullet.prog);
    }

    // Finish
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
