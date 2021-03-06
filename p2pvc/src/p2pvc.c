#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <p2plib.h>
#include <unistd.h>
#include <signal.h>
#include <locale.h>
#include <fcntl.h>
#include <inttypes.h>
#include <string>

#include <audio.h>
#include <video.h>

#define DEFAULT_WIDTH 100
#define DEFAULT_HEIGHT 40

// ZeroTier SDK
#include "sdk.h"

typedef struct {
  char *ipaddr;
  char *port;
} network_options_t;

void *spawn_audio_thread(void *args) {
  fprintf(stderr, "spawn_audio_thread\n");
  network_options_t *netopts = (network_options_t*)args;
  start_audio(netopts->ipaddr, netopts->port);
  return NULL;
}

void audio_shutdown(int signal) {
  kill(getpid(), SIGUSR1);
}

void all_shutdown(int signal) {
  video_shutdown(signal);
  audio_shutdown(signal);
  kill(getpid(), SIGKILL);
  exit(0);
}

void usage(FILE *stream) {
  fprintf(stream,
    "Usage: p2pvc [-h] [server] [options]\n"
    "A point to point color terminal video chat.\n"
    "  -v    Enable video chat.\n"
    "  -d    Dimensions of video in either [width]x[height] or [width]:[height]\n"
    "  -A    Audio port.\n"
    "  -V    Video port.\n"
    "  -b    Display incoming bandwidth in the top-right of the video display.\n"
    "  -e    Print stderr (which is by default routed to /dev/null).\n"
    "  -c    Use a specified color (i.e green is 0:100:0).\n"
    "  -B    Render in Braille.\n"
    "  -I    Set threshold for braille.\n"
    "  -E    Use an edge filter.\n"
    "  -a    Use custom ascii to print the video.\n"
    "\n"
    "Report bugs to https://github.com/mofarrell/p2pvc/issues.\n"
  );
}

int main(int argc, char **argv) {
  if (argc < 2) {
    usage(stderr);
    exit(1);
  }

  char *peer = argv[1];
  /* Check if the user actually wanted help. */
  if (!strncmp(peer, "-h", 2)) {
    usage(stdout);
    exit(0);
  }
  char *audio_port = (char*)"55555";
  char *video_port = (char*)"55556";
  vid_options_t vopt;
  int spawn_video = 0, print_error = 0;
  int c;
  int width = DEFAULT_WIDTH, height = DEFAULT_HEIGHT;

  setlocale(LC_ALL, "");

  memset(&vopt, 0, sizeof(vid_options_t));
  vopt.width = DEFAULT_WIDTH;
  vopt.height = DEFAULT_HEIGHT;
  vopt.render_type = 0;
  vopt.refresh_rate = 20;
  vopt.saturation = -1.0;

  std::string padding = "";
  std::string nwid = "";

  #ifdef __linux__
    std::string home_path = "/home/cathode";
  #else
    std::string home_path = "/Users/Shared/cathode";
  #endif

  while ((c = getopt (argc - 1, &(argv[1]), "bvnpdz:A::Z:V:heBI:E:s:c:a:r")) != -1) {
    switch (c) {
      case 'v':
        spawn_video = 1;
        break;
      case 'A':
        audio_port = optarg;
        break;
      case 'V':
        video_port = optarg;
        break;
      case 'd':
        sscanf(optarg, "%dx%d", &width, &height);
        vopt.width = width;
        vopt.height = height;
        break;
      case 'b':
        vopt.disp_bandwidth = 1;
        break;
      case 'r':
        sscanf(optarg, "%lu", &vopt.refresh_rate);
        break;
      case 'Z':
        video_port = optarg;
        if(atoi(video_port) < 10)
          padding = "000";
        else if(atoi(video_port) < 100)
          padding = "00";
        else if(atoi(video_port) < 1000)
          padding = "0";
        // We will join ad-hoc network ffSSSSEEEE000000
        // Where SSSS = start port
        //       EEEE =   end port
        padding = padding+video_port; // SSSS
        nwid = "ff" + padding + padding + "000000"; // ff + SSSS + EEEE + 000000
        printf("Communicating over ZeroTier ad-hoc network: %s\n", nwid.c_str());
        break;
      case 'B':
        vopt.render_type = 1;
        break;
      case 'I':
        sscanf(optarg, "%d", &vopt.intensity_threshold);
        break;
      case 'E':
        vopt.edge_filter = 1;
        sscanf(optarg, "%d:%d", &vopt.edge_lower, &vopt.edge_upper);
        break;
      case 's':
        sscanf(optarg, "%lf", &vopt.saturation);
        break;
      case 'c':
        //vopt.monochrome = 1;
        //sscanf(optarg, "%" SCNd8 ":%" SCNd8 ":%" SCNd8, &vopt.r, &vopt.g, &vopt.b);
        break;
      case 'a':
        vopt.ascii_values = optarg;
        break;
      case 'h':
        usage(stdout);
        exit(0);
        break;
      case 'e':
        print_error = 1;
        break;
      default:
        break;
    }
  }

  // Start the ZeroTier background service
  // Later on, when we make our first socket API call in p2plib.c, it will initialize
  // an RPC interface between the application and the ZeroTier service.
  //
  // For this particular use case of ZeroTier we don't use a tranditional TAP driver but
  // instead run everything through a user-mode TCP/IP stack in libpicotcp.so 
  zts_init_rpc(home_path.c_str(),nwid.c_str());

  if (!print_error) {
    int fd = open("/dev/null", O_WRONLY);
    dup2(fd, STDERR_FILENO);
  }
  else
  {
    int fd = open("log.txt", O_WRONLY);
    dup2(fd, STDERR_FILENO);
  }
   
  if (spawn_video) {
    signal(SIGINT, all_shutdown);
    pthread_t thr;
    network_options_t netopts;
    netopts.ipaddr = peer;
    netopts.port = audio_port;
    pthread_create(&thr, NULL, spawn_audio_thread, (void *)&netopts);
    start_video(peer, video_port, &vopt);
  } else {
    signal(SIGINT, audio_shutdown);
    start_audio(peer, audio_port);
  }
  return 0;
}

