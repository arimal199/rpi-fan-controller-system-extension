// fan_controller.c - Minimal GPIO12 fan controller for Talos Linux
#include <errno.h>
#include <fcntl.h>
#include <linux/gpio.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define GPIOCHIP "/dev/gpiochip0"
#define GPIO_LINE 12
#define TEMP_PATH "/sys/class/thermal/thermal_zone0/temp"
#define DEFAULT_THRESHOLD 45000
#define DEFAULT_HYSTERESIS 2000
#define POLL_INTERVAL_SEC 3

static volatile int running = 1;
static FILE *temp_file = NULL;
static int gpio_fd = -1;

void cleanup(int sig) { running = 0; }

int setup_gpio() {
  gpio_fd = open(GPIOCHIP, O_RDWR);
  if (gpio_fd < 0) {
    perror("Failed to open gpiochip");
    return -1;
  }

  struct gpio_v2_line_request req = {0};
  req.offsets[0] = GPIO_LINE;
  req.num_lines = 1;
  req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
  strcpy(req.consumer, "fan");

  if (ioctl(gpio_fd, GPIO_V2_GET_LINE_IOCTL, &req) < 0) {
    perror("GPIO_V2_GET_LINE_IOCTL failed");
    close(gpio_fd);
    return -1;
  }

  close(gpio_fd);   // Close original
  gpio_fd = req.fd; // Use returned line fd
  return 0;
}

int read_temp() {
  if (!temp_file) {
    temp_file = fopen(TEMP_PATH, "r");
    if (!temp_file) {
      perror("Failed to open temperature file");
      return -1;
    }
  }

  rewind(temp_file);
  clearerr(temp_file);

  int temp;
  if (fscanf(temp_file, "%d", &temp) != 1) {
    perror("Failed to read temperature");
    fclose(temp_file);
    temp_file = NULL;
    return -1;
  }
  return temp;
}

void set_fan(int state) {
  if (gpio_fd < 0)
    return;

  struct gpio_v2_line_values values = {0};
  values.mask = 1;
  values.bits = state;

  if (ioctl(gpio_fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0) {
    perror("Failed to set GPIO value");
  }
}

int main(int argc, char *argv[]) {
  int threshold = DEFAULT_THRESHOLD;
  int hysteresis = DEFAULT_HYSTERESIS;

  if (argc > 1)
    threshold = atoi(argv[1]);
  if (argc > 2)
    hysteresis = atoi(argv[2]);

  printf("Fan controller starting...\n");
  printf("  Threshold : %d\n", threshold);
  printf("  Hysteresis: %d\n", hysteresis);

  signal(SIGINT, cleanup);
  signal(SIGTERM, cleanup);

  if (setup_gpio() < 0) {
    fprintf(stderr, "GPIO setup failed\n");
    return 1;
  }

  int fan_state = 0;

  struct timespec delay = {.tv_sec = POLL_INTERVAL_SEC, .tv_nsec = 0};

  while (running) {
    int temp = read_temp();
    if (temp < 0) {
      nanosleep(&delay, NULL);
      continue;
    }

    int new_state = fan_state ? (temp < threshold - hysteresis ? 0 : 1)
                              : (temp >= threshold ? 1 : 0);

    if (new_state != fan_state) {
      set_fan(new_state);
      fan_state = new_state;
      printf("Fan turned %s at %d m°C\n", fan_state ? "ON" : "OFF", temp);
    }

    nanosleep(&delay, NULL);
  }

  printf("Exiting...\n");

  if (temp_file)
    fclose(temp_file);
  if (gpio_fd >= 0)
    close(gpio_fd);
  return 0;
}
