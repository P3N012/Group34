#pragma once
/* Serial command line: monitoring commands for everyone, settings commands only after "login". */

void cli_init(void);                  /* install the UART driver and start the console task */
void cli_run_command(char *line);     /* run one command line (also used by the PC tests) */
