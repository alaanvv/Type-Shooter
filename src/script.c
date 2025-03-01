#include "canvas.h"

#define IS_UPPER(c) (c >= 'A' && c <= 'Z')
#define TO_UPPER(c) { c += 'A' - 'a'; }
#define TO_LOWER(c) { c += 'a' - 'A'; }

#define UPSCALE 1

#define ENEMY_ROWS  5
#define ENEMY_COLS  5
#define STAR_AMOUNT 200

#define MAX_OFFSET       0.6 * ENEMY_COLS
#define ENEMIES_DISTANCE 20
#define ENEMY_SPACE      4
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

void init_game() {
  srand((uintptr_t) &cam);
  for (u8 i = 0; i < STAR_AMOUNT; i++)
    VEC3_COPY(VEC3(RAND(-60, 60), RAND(-60, 60), -RAND(50, 60)), stars[i]);

  for (u8 i = 0; i < ENEMY_ROWS; i++)
    enemies[0][i].next_shoot = glfwGetTime() + RAND(3, 10);
}

void game_tick() {
  for (u8 i = 0; i < ENEMY_ROWS; i++)
    for (u8 j = 0; j < ENEMY_COLS; j++)
      VEC3_COPY(VEC3((j - (ENEMY_COLS - 1) / 2.0) * ENEMY_SPACE + x_offset * (i % 2 ? 1 : -1), i * ROW_HEIGHT, i * ROW_SPACE), enemies[i][j].pos);

  for (u8 i = 0; i < ENEMY_COLS; i++) {
    if (enemies[0][i].next_shoot - glfwGetTime() < 0 && !enemies[0][i].bullet.active && enemies[0][i].word[0]) {
      enemies[0][i].next_shoot = glfwGetTime() + RAND(20, 40);
      enemies[0][i].bullet.active = 1;
      VEC3_COPY(enemies[0][i].pos, enemies[0][i].bullet.pos);
      enemies[0][i].bullet.rotation = atan2f(-enemies[0][i].bullet.pos[0], ENEMIES_DISTANCE-enemies[0][i].bullet.pos[2]);
    }

    if (enemies[0][i].bullet.active) {
      vec3 dir;
      glm_vec3_sub(VEC3(0.5 * (i-(ENEMY_COLS/2.0)), -1, ENEMIES_DISTANCE), enemies[0][i].bullet.pos, dir);
      glm_normalize(dir);
      glm_vec3_scale(dir, 0.05, dir);
      glm_vec3_add(enemies[0][i].bullet.pos, dir, enemies[0][i].bullet.pos);
      if (enemies[0][i].bullet.pos[2] >= ENEMIES_DISTANCE) {
        enemies[0][i].bullet.active = 0;
        glfwSetWindowShouldClose(cam.window, 1);
      }
    }
  }

  if (recoil) recoil = MAX(recoil - 0.001, 0);

  if (x_offset >  MAX_OFFSET && dir ==  1) dir = -1;
  if (x_offset < -MAX_OFFSET && dir == -1) dir =  1;
  x_offset += dir * 0.05;

  for (u8 i = 0; i < ENEMY_COLS; i++) {
    if (!bullets[0][i].active) continue;
    vec3 dir;
    glm_vec3_sub(enemies[0][i].pos, bullets[0][i].pos, dir);
    glm_normalize(dir);
    glm_vec3_scale(dir, 0.8, dir);
    glm_vec3_add(bullets[0][i].pos, dir, bullets[0][i].pos);
    bullets[0][i].rotation = atan2f(enemies[0][i].pos[0] - bullets[0][i].pos[0], -bullets[0][i].pos[2]);
    if (bullets[0][i].pos[2] <= 0) bullets[0][i].active = 0;
  }

  for (u8 i = 0; i < ENEMY_COLS; i++) {
    if (!bullets[1][i].active) continue;
    vec3 dir;
    glm_vec3_sub(enemies[0][i].bullet.pos, bullets[1][i].pos, dir);
    glm_normalize(dir);
    glm_vec3_scale(dir, 0.8, dir);
    glm_vec3_add(bullets[1][i].pos, dir, bullets[1][i].pos);
    bullets[1][i].rotation = atan2f(enemies[0][i].bullet.pos[0] - bullets[1][i].pos[0], enemies[0][i].bullet.pos[2]-bullets[1][i].pos[2]);
    if (bullets[1][i].pos[2] - enemies[0][i].bullet.pos[2] <= 0) {
      bullets[1][i].active = 0;
      enemies[0][i].bullet.active = 0;
    }
  }
}

void char_press(GLFWwindow* window, u32 key) {
  for (u8 i = 0; i < ENEMY_COLS; i++)
    for (u8 j = 0; enemies[0][i].word[j]; j++) {
      if (IS_UPPER(enemies[0][i].word[j])) continue;

      else if (enemies[0][i].word[j] == key) {
        TO_UPPER(enemies[0][i].word[j]);
        enemies[0][i].prog += 0.1;
        if (!enemies[0][i].word[j + 1]) {
          enemies[0][i].word[0] = 0;
          bullets[0][i].pos[0] = 0;
          bullets[0][i].pos[1] = -3;
          bullets[0][i].pos[2] = 15;
          bullets[0][i].active = 1;
          recoil += 0.02;
        }
      }

      else {
        for (i8 k = j - 1; k >= 0; k--)
          if (k != 0 || (enemies[0][i].word[k] + ('a' - 'A')) != key)
          TO_LOWER(enemies[0][i].word[k]);
        enemies[0][i].prog=0;
      }

      break;
    }
  for (u8 i = 0; i < ENEMY_COLS; i++)
    for (u8 j = 0; enemies[0][i].bullet.word[j]; j++) {
      if (IS_UPPER(enemies[0][i].bullet.word[j])) continue;

      else if (enemies[0][i].bullet.word[j] == key) {
        TO_UPPER(enemies[0][i].bullet.word[j]);
        enemies[0][i].bullet.prog += 0.1;
        if (!enemies[0][i].bullet.word[j + 1]) {
          enemies[0][i].bullet.word[0] = 0;
          bullets[1][i].pos[0] = 0;
          bullets[1][i].pos[1] = -3;
          bullets[1][i].pos[2] = 15;
          bullets[1][i].active = 1;
          recoil += 0.02;
        }
      }

      else {
        for (i8 k = j - 1; k >= 0; k--)
          if (k != 0 || (enemies[0][i].bullet.word[k] + ('a' - 'A')) != key)
            TO_LOWER(enemies[0][i].bullet.word[k]);
        enemies[0][i].bullet.prog=0;
      }

      break;
    }
}

int main() {
  init_game();
  canvas_init(&cam, (CanvasInitConfig) { "Type Shooter", 1, 0, 0.8, { 0.05, 0, 0.05 } });
  glfwSetCharCallback(cam.window, char_press);

  // Material
  Material m_ship   = { GRAY,          0.5, 0.5 };
  Material m_bullet = { PURPLE, 0.5, 0.5 };
  Material m_enbullet = { PASTEL_YELLOW, 0.5, 0.5 };
  Material m_enemy  = { DEEP_GREEN,    0.5, 0.5 };
  Material m_star   = { WHITE,         .lig = 1 };
  Material m_text   = { PASTEL_GREEN,  .lig = 1 };
  Material m_text_w = { PURPLE,        .lig = 1 };

  // Light
  DirLig light = { WHITE, { 0, 0 , -1 } };

  // Model
  Model* enbullet = model_create("obj/cube.obj",  &m_enbullet, 1);
  Model* bullet = model_create("obj/cube.obj",  &m_bullet, 1);
  Model* star   = model_create("obj/star.obj",  &m_star,   1);
  Model* ship   = model_create("obj/ship.obj",  &m_ship,   1);
  Model* enemy  = model_create("obj/enemy.obj", &m_enemy,  1);

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

    // 3D Drawing
    glUseProgram(shader);

    model_bind(ship, shader);
    glm_translate(ship->model, VEC3(0, 0, ENEMIES_DISTANCE + recoil));
    model_draw(ship, shader);

    for (u8 i = 0; i < ENEMY_ROWS; i++)
      for (u8 j = 0; j < ENEMY_COLS; j++) {
        if (i == 0 && !enemies[0][j].word[0] && !bullets[0][j].active) continue;
        model_bind(enemy, shader);
        glm_translate(enemy->model, enemies[i][j].pos);
        model_draw(enemy, shader);

        if (enemies[0][j].bullet.active) {
          model_bind(enbullet, shader);
          glm_translate(enbullet->model, enemies[0][j].bullet.pos);
          glm_rotate(enbullet->model, enemies[0][j].bullet.rotation, VEC3(0, 1, 0));
          glm_rotate(enbullet->model, PI4/8, VEC3(1, 0, 0));
          glm_scale(enbullet->model, VEC3(0.5, 0.5, 1));
          model_draw(enbullet, shader);
        }

        if (bullets[0][j].active) {
          model_bind(bullet, shader);
          glm_translate(bullet->model, bullets[0][j].pos);
          glm_rotate(bullet->model, bullets[0][j].rotation, VEC3(0, 1, 0));
          glm_scale(bullet->model, VEC3(0.5, 0.5, 3));
          model_draw(bullet, shader);
        }

        if (bullets[1][j].active) {
          model_bind(bullet, shader);
          glm_translate(bullet->model, bullets[1][j].pos);
          glm_rotate(bullet->model, bullets[1][j].rotation, VEC3(0, 1, 0));
          glm_scale(bullet->model, VEC3(0.5, 0.5, 3));
          model_draw(bullet, shader);
        }
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
      c8 typed[32];
      u8 j = 0;
      for (; IS_UPPER(enemies[0][i].word[j]); j++) typed[j] = enemies[0][i].word[j];
      typed[j] = 0;

      f32 x = enemies[0][i].pos[0] - canvas_text_width(enemies[0][i].word, font, 0.01) / 2 + sin(glfwGetTime()*(1+enemies[0][i].prog) * 10) * enemies[0][i].prog * 0.1;
      canvas_draw_text(shader, typed,        x,                                        -1.3 + cos(glfwGetTime()*(2+enemies[0][i].prog) * 8) * enemies[0][i].prog * 0.13, 0, 0.01, font, m_text_w, VEC3(0, 0, 0));
      canvas_draw_text(shader, &enemies[0][i].word[j], x + canvas_text_width(typed, font, 0.01), -1.3 + cos(glfwGetTime()*(2+enemies[0][i].prog) * 8) * enemies[0][i].prog * 0.13, 0, 0.01, font, m_text,   VEC3(0, 0, 0));
    }

    for (u8 i = 0; i < ENEMY_COLS; i++) {
      if (!enemies[0][i].bullet.active) continue;
      c8 typed[32];
      u8 j = 0;
      for (; IS_UPPER(enemies[0][i].bullet.word[j]); j++) typed[j] = enemies[0][i].bullet.word[j];
      typed[j] = 0;

      f32 x = enemies[0][i].bullet.pos[0] - canvas_text_width(enemies[0][i].bullet.word, font, 0.01) / 2 + sin(glfwGetTime()*(1+enemies[0][i].bullet.prog) * 10) * enemies[0][i].bullet.prog * 0.1;
      canvas_draw_text(shader, typed,        x,                                         enemies[0][i].bullet.pos[1]-1.3 + cos(glfwGetTime()*(2+enemies[0][i].bullet.prog) * 8) * enemies[0][i].bullet.prog * 0.13, enemies[0][i].bullet.pos[2], 0.01, font, m_text_w, VEC3(0, 0, 0));
      canvas_draw_text(shader, &enemies[0][i].bullet.word[j], x + canvas_text_width(typed, font, 0.01), enemies[0][i].bullet.pos[1]-1.3 + cos(glfwGetTime()*(2+enemies[0][i].bullet.prog) * 8) * enemies[0][i].bullet.prog * 0.13, enemies[0][i].bullet.pos[2], 0.01, font, m_text,   VEC3(0, 0, 0));
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
