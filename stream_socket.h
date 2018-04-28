void print_err(char* msg);

char* get_hostname_from_arg(char* arg);

int get_portno_from_arg(char* arg);

int sendPacketTCP(int socket, char* data, int data_len);

int receivePacketTCP(int socket, char* data);

int sendPacketUDP(int socket, char* data, int data_len);

int receivePacketUDP(int socket, char* data);
