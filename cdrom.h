


int cdromOpenIso(char *path);
int cdromReadBlock(void *buffer,int sector);
int cdromCloseIso();
unsigned int cdromDiscSize();
