#include "server.h"
#include "client.h"
#include <cstring>
#include <iostream>
#include <string>
#include <getopt.h>

int main(int argc, char* argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <server|echo> <args...>"
              << std::endl;
    return 1;
  }

  std::string mode = argv[1];
  int connection_count = 0;
  std::string address;
  int port = 0;

  if (mode == "server") {
    port = std::stoi(argv[2]);
    run_server(port);
  } else if (mode == "echo") {
    int opt;
    while ((opt = getopt(argc - 1, argv + 1, "c:s:")) != -1) {
      switch (opt) {
        case 'c':
          connection_count = std::stoi(optarg);
          break;
        case 's':
          address = optarg;
          break;
        default:
          std::cerr << "Invalid arguments." << std::endl;
          return 1;
      }
    }

    if (connection_count > 0 && !address.empty()) {
      run_client(connection_count, address);
    } else {
      std::cerr << "Both -c and -s options are required." << std::endl;
      return 1;
    }
  } else {
    std::cerr << "Invalid mode: " << mode << std::endl;
    return 1;
  }

  return 0;
}
