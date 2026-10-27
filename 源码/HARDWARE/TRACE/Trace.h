#ifndef __Trace_H
#define __Trace_H

/*
 * 5-channel gray sensors, left to right:
 * X1 -> PA15, X2 -> PA6, X3 -> PB1, X4 -> PA4, X5 -> PB12
 * PA15 needs JTAG disabled in Trace_Init(), SWD remains available.
 */
void Trace_Init(void);

#endif
