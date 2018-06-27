extern unsigned char mem[];
extern unsigned int cpucycles;
extern unsigned short pc;
void initcpu(unsigned short newpc, unsigned char newa, unsigned char newx, unsigned char newy);
int runcpu(void);
