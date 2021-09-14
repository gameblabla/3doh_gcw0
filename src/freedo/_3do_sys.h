#ifndef THREEDO_SYS_H
#define THREEDO_SYS_H

extern int _3do_Init(void);

extern struct VDLFrame *curr_frame;
extern bool skipframe;

extern void _3do_InternalFrame(int cycles);
extern void _3do_Frame(struct VDLFrame *frame, bool __skipframe);
extern void _3do_Destroy();

extern uint32_t _3do_SaveSize(void);

extern void _3do_Save(void *buff);

extern bool _3do_Load(void *buff);
extern void _3do_OnSector(uint32_t sector);

extern void _3do_Read2048(void *buff);
extern uint32_t _3do_DiscSize(void);

#endif
