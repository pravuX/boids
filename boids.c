#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WINDOW_WIDTH 1900
#define WINDOW_HEIGHT 1000
#define CELL_SIZE 100
#define WINDOW_ROWS (WINDOW_WIDTH / CELL_SIZE)
#define WINDOW_COLS (WINDOW_HEIGHT / CELL_SIZE)

#define SIZE (WINDOW_ROWS * WINDOW_COLS)
#define MAX_VELOCITY 6
#define MIN_VELOCITY 3

#define MARGIN 150
#define LEFT_MARGIN (MARGIN)
#define RIGHT_MARGIN (WINDOW_WIDTH - MARGIN)
#define TOP_MARGIN (MARGIN)
#define BOTTOM_MARGIN (WINDOW_HEIGHT - MARGIN)

#define FLOCK_SIZE 1900
#define OBSTACLE_SIZE 5
const float turn_factor = 0.2;
const int visual_range = 80;
const int vicinity = visual_range + 20;
const int protected_range = 10;
const float centering_factor = 0.0008;
// const float avoidfactor = 0.5; // faster but more chaotic
const float avoidfactor = 0.1; // tighter flocks but slower
// const float avoidfactor = 0.275; // balanced
const float matching_factor = 0.05;

const int obstacle_range = 170;
const float obstacle_turn_factor = 0.3;

const Color vicinity_color = {59, 69, 129, 255};

const Color colors[10] = {
  LIGHTGRAY,
  DARKGRAY,
  YELLOW,
  GOLD,
  PINK,
  SKYBLUE,
  BLUE,
  PURPLE,
  VIOLET,
  RAYWHITE
};

typedef struct {
  float x;
  float y;
  float vx;
  float vy;
  int id;
  int color;
} Boid;

typedef struct {
  int x;
  int y;
} Obstacle;

Boid boids[FLOCK_SIZE];
Obstacle obstacles[OBSTACLE_SIZE];

float max(float x, float y) {
  if (x > y)
    return x;
  return y;
}
float min(float x, float y) {
  if (x < y)
    return x;
  return y;
}

float pythagorean_addition(float x, float y) {
  float Max = max(fabs(x), fabs(y));
  float Min = min(fabs(x), fabs(y));
  float a0 = 127.0 / 128.0;
  float b0 = 3.0 / 16.0;
  float a1 = 27.0 / 32.0;
  float b1 = 71.0 / 128.0;
  ;
  return max(a0 * Max + b0 * Min, a1 * Max + b1 * Min);
}

void initialize_boids() {
  for (int i = 0; i < FLOCK_SIZE; i++) {
    Boid boid;
    boid.x = (float)(random() % WINDOW_WIDTH);
    boid.y = (float)(random() % WINDOW_HEIGHT);
    boid.vy = (float)(random() % (2 * MAX_VELOCITY) - MAX_VELOCITY);
    boid.vx = (float)(random() % (2 * MAX_VELOCITY) - MAX_VELOCITY);
    boid.id = i;
    boid.color = random() % 10;

    boids[i] = boid;
  }
}

void initialize_obstacles() {
  for (int i = 0; i < OBSTACLE_SIZE; i++) {
    Obstacle obs;
    obs.x = random() % (RIGHT_MARGIN - MARGIN) + MARGIN;
    obs.y = random() % (BOTTOM_MARGIN - MARGIN) + MARGIN;
    obstacles[i] = obs;
  }
}

typedef struct Entry {
  int key;
  int value;
  struct Entry *next;
} Entry;

// may be the hash table is just an array?
typedef struct {
  Entry **cells;
  int size;
} SpatialHashTable;

// returns the key for the cell corresponding the
// boid position
int hash(float b_x, float b_y) {
  int x = (int)(b_x / CELL_SIZE);
  int y = (int)(b_y / CELL_SIZE);
  return x + y * WINDOW_COLS;
}

SpatialHashTable *create_hash_table() {
  int size = SIZE;
  SpatialHashTable *h = malloc(sizeof(SpatialHashTable));
  if (h != NULL) {
    h->size = size;
    h->cells = calloc(size, sizeof(Entry *));
    if (h->cells == NULL) {
      printf("Error! Failed to allocate cell\n");
      exit(1);
    }
  } else {
    printf("Error! Failed to Allocate HashTable\n");
    exit(1);
  }
  return h;
}

void add_to_hash_table(SpatialHashTable *h, int key, int value) {
  // Always adds to the head of the linked list
  Entry *cell = h->cells[key];
  Entry *new = malloc(sizeof(Entry));
  if (new == NULL) {
    printf("Error! Failed to allocate Entry\n");
    exit(1);
  }
  new->key = key;
  new->value = value;
  h->cells[key] = new;
  if (cell != NULL) {
    new->next = cell;
  }
}

void fill_hash_table(SpatialHashTable *h) {
  for (int i = 0; i < FLOCK_SIZE; i++) {
    int key = hash(boids[i].x, boids[i].y);
    int value = boids[i].id;
    add_to_hash_table(h, key, value);
  }
}

void remove_from_cell(SpatialHashTable *h, int key, int value) {
  Entry *cell = h->cells[key];
  while (cell != NULL) {
    // if the value is in the head
    if (cell->value == value) {
      h->cells[key] = cell->next;
      free(cell);
      return;
    }
    // if the value is anywhere in the middle or the end
    if (cell->next->value == value) {
      Entry *matching_node = cell->next;
      cell->next = matching_node->next;
      free(matching_node);
      return;
    }
    cell = cell->next;
  }
}

void update_hash_table(SpatialHashTable *h, int value, int prev_key,
                       int new_key) {
  // remove the value from previous cell
  remove_from_cell(h, prev_key, value);
  // add the value into the new key
  add_to_hash_table(h, new_key, value);
}

void free_hash_table(SpatialHashTable *h) {
  for (int key = 0; key < h->size; key++) {
    Entry *cell = h->cells[key];
    while (cell != NULL) {
      Entry *next = cell->next;
      free(cell);
      cell = next;
    }
  }
  free(h->cells);
  free(h);
}

void print_hash_table(SpatialHashTable *h) {
  for (int key = 0; key < h->size; key++) {
    Entry *cell = h->cells[key];
    printf("cell %d: ", key);
    while (cell != NULL) {
      printf("(%d, %d) -> ", cell->key, cell->value);
      cell = cell->next;
    }
    printf("NULL\n");
  }
}

float clamp(float a, float min, float max) {
  if (a <= min)
    return min;
  if (a >= max)
    return max;
  return a;
}

int get_neighboring_boids(SpatialHashTable *h, Boid boid, int *neighbors) {
  float b_x = boid.x;
  float b_y = boid.y;

  int neighbor_count = 0;

  int minx = (int)clamp(b_x - vicinity / 2.0, 0, WINDOW_WIDTH - 1) / CELL_SIZE;
  int miny = (int)clamp(b_y - vicinity / 2.0, 0, WINDOW_HEIGHT - 1) / CELL_SIZE;
  int maxx = (int)clamp(b_x + vicinity / 2.0, 0, WINDOW_WIDTH - 1) / CELL_SIZE;
  int maxy = (int)clamp(b_y + vicinity / 2.0, 0, WINDOW_HEIGHT - 1) / CELL_SIZE;

  // for visualizing the immediate vicinity of each boid
  /* int left = (int)b_x-vicinity/2;
  int top = (int)b_y-vicinity/2;
  int right = (int)b_x+vicinity/2;
  int bottom = (int)b_y+vicinity/2;
  int width = right - left;
  int height = bottom - top;
  DrawRectangleLines(left, top, width, height, vicinity_color); */

  for (int x = minx; x <= maxx; x++) {
    for (int y = miny; y <= maxy; y++) {
      int key = x + y * WINDOW_COLS;
      Entry *cell = h->cells[key];
      while (cell != NULL) {
        neighbors[neighbor_count] = cell->value;
        neighbor_count += 1;
        cell = cell->next;
      }
    }
  }

  return neighbor_count;
}

int main(void) {
  srand(time(NULL));
  initialize_boids();
  initialize_obstacles();
  SpatialHashTable *hash_table = create_hash_table();
  fill_hash_table(hash_table);

  InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "boids");

  SetTargetFPS(120);

  while (!WindowShouldClose()) {

    BeginDrawing();
    ClearBackground(BLACK);

    /* DrawRectangleLines(LEFT_MARGIN, TOP_MARGIN, RIGHT_MARGIN - MARGIN,
                       BOTTOM_MARGIN - MARGIN, WHITE); */

    for (int i = 0; i < FLOCK_SIZE; i++) {
      DrawCircle(boids[i].x, boids[i].y, 3.0f, colors[boids[i].color]);
      DrawLine(boids[i].x, boids[i].y, boids[i].x - boids[i].vx * 5,
               boids[i].y - boids[i].vy * 5, colors[boids[i].color]);
    }
    for (int i = 0; i < OBSTACLE_SIZE; i++) {
      int x = obstacles[i].x;
      int y = obstacles[i].y;
      DrawCircle(x, y, 30.0f, RED);
      DrawCircleLines(x - 10, y, 10.0f, BLACK);
      DrawCircleLines(x + 10, y, 10.0f, BLACK);
      DrawCircleLines(x, y - 10, 10.0f, BLACK);
      DrawCircleLines(x, y + 10, 10.0f, BLACK);
    }

    EndDrawing();

    for (int i = 0; i < FLOCK_SIZE; i++) {
      float xpos_avg = 0.0f;
      float ypos_avg = 0.0f;
      float xvel_avg = 0.0f;
      float yvel_avg = 0.0f;

      int neighboring_boids = 0;
      float close_dx = 0.0f;
      float close_dy = 0.0f;

      int num_obstacles = 0;
      float obstacle_dx = 0;
      float obstacle_dy = 0;

      int neighbors[FLOCK_SIZE];
      int boids_in_vicinity_count;
      int neighbor_index;
      boids_in_vicinity_count =
          get_neighboring_boids(hash_table, boids[i], neighbors);

      for (int j = 0; j < boids_in_vicinity_count; j++) {

        neighbor_index = neighbors[j];

        if (boids[i].id != boids[neighbor_index].id) {
          // Uncomment to allow flocking with similar boids
          // if (boids[i].color == boids[neighbor_index].color) {

            float dx = boids[i].x - boids[neighbor_index].x;
            float dy = boids[i].y - boids[neighbor_index].y;

            float squared_distance = dx * dx + dy * dy;

            if (squared_distance < protected_range * protected_range) {
              close_dx += boids[i].x - boids[neighbor_index].x;
              close_dy += boids[i].y - boids[neighbor_index].y;
            }
          if (boids[i].color == boids[neighbor_index].color) {
            xpos_avg += boids[neighbor_index].x;
            ypos_avg += boids[neighbor_index].y;
            xvel_avg += boids[neighbor_index].vx;
            yvel_avg += boids[neighbor_index].vy;
            neighboring_boids += 1;
          }
        }
      }

      if (neighboring_boids > 0) {
        xpos_avg /= neighboring_boids;
        ypos_avg /= neighboring_boids;
        xvel_avg /= neighboring_boids;
        yvel_avg /= neighboring_boids;
        boids[i].vx += (xpos_avg - boids[i].x) * centering_factor +
                       (xvel_avg - boids[i].vx) * matching_factor;
        boids[i].vy += (ypos_avg - boids[i].y) * centering_factor +
                       (yvel_avg - boids[i].vy) * matching_factor;
      }
      boids[i].vx += close_dx * avoidfactor;
      boids[i].vy += close_dy * avoidfactor;

      for (int j = 0; j < OBSTACLE_SIZE; j++) {
        float dx = boids[i].x - (float)obstacles[j].x;
        float dy = boids[i].y - (float)obstacles[j].y;
        if (fabs(dx) < obstacle_range && fabs(dy) < obstacle_range) {
          float squared_distance = dx * dx + dy * dy;
          if (squared_distance < obstacle_range * obstacle_range) {
            obstacle_dx += boids[i].x - (float)obstacles[j].x;
            obstacle_dy += boids[i].y - (float)obstacles[j].y;
            num_obstacles += 1;
          }
        }
      }

      if (num_obstacles > 0) {
        if (obstacle_dy > 0)
          boids[i].vy += obstacle_turn_factor;
        if (obstacle_dy < 0)
          boids[i].vy -= obstacle_turn_factor;
        if (obstacle_dx > 0)
          boids[i].vx += obstacle_turn_factor;
        if (obstacle_dx < 0)
          boids[i].vx -= obstacle_turn_factor;
      }

      if (boids[i].x < LEFT_MARGIN) {
        boids[i].vx += turn_factor;
      }
      if (boids[i].x > RIGHT_MARGIN) {
        boids[i].vx -= turn_factor;
      }
      if (boids[i].y < TOP_MARGIN) {
        boids[i].vy += turn_factor;
      }
      if (boids[i].y > BOTTOM_MARGIN) {
        boids[i].vy -= turn_factor;
      }

      float mag = pythagorean_addition(boids[i].vx, boids[i].vy);
      if (mag > MAX_VELOCITY) {
        boids[i].vx = (boids[i].vx / mag) * MAX_VELOCITY;
        boids[i].vy = (boids[i].vy / mag) * MAX_VELOCITY;
      }
      if (mag < MIN_VELOCITY && mag != 0.0) {
        boids[i].vx = (boids[i].vx / mag) * MIN_VELOCITY;
        boids[i].vy = (boids[i].vy / mag) * MIN_VELOCITY;
      }

      int prev_key = hash(boids[i].x, boids[i].y);

      boids[i].x += boids[i].vx;
      boids[i].y += boids[i].vy;

      if (boids[i].x > WINDOW_WIDTH)
        boids[i].x = 0.0f;
      if (boids[i].x < 0 || isnan(boids[i].x))
        boids[i].x = WINDOW_WIDTH;
      if (boids[i].y > WINDOW_HEIGHT)
        boids[i].y = 0.0f;
      if (boids[i].y < 0 || isnan(boids[i].y))
        boids[i].y = WINDOW_HEIGHT;

      int new_key = hash(boids[i].x, boids[i].y);
      update_hash_table(hash_table, boids[i].id, prev_key, new_key);
    }
  }

  free_hash_table(hash_table);
  CloseWindow();
  return 0;
}
