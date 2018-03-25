void print_err(char* msg);

char* get_hostname_from_arg(char* arg);

int get_portno_from_arg(char* arg);

int sendPacket(int socket, char* data, int data_len);

int receivePacket(int socket, char* data);

