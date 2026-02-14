# Documentación

1) Vector Table (`root.s`)

- La vector table tiene como propósito proporcionar un "indice" fijo de instrucciones de salto que la CPU utiliza cuando ocurre una excepción o interrupción. Cada entrada de la vector table contiene una instrucción que salta a un manejador específico.
- Se utiliza un `.align 5` para alinear la tabla a 2^5 = 32 bytes. Esto porque ARM espera que las vector table estén alineadas para que el hardware pueda acceder a ellas de manera predecible.

- Las entradas de la vector table y sus offsets son:
   - 0x00 Reset
   - 0x04 Undefined Instruction
   - 0x08 SWI (Software Interrupt)
   - 0x0C Prefetch Abort
   - 0x10 Data Abort
   - 0x14 Reserved
   - 0x18 IRQ
   - 0x1C FIQ

2)  `mcr` 

- El código base que nos dieron contenía la instrucción `mcr p15, 0, r0, c12, c0, 0` que es una instrucción ARM para escribir un valor desde un registro ARM a un registro de coprocessador o de control del sistema. La sintaxis es: `MCR <coproc>, <opc1>, <Rt>, <CRn>, <CRm>, <opc2>`, en este caso, está escribiendo el valor en `r0` (que contiene la dirección de la vector table) al registro CP15 que es el que controla la dirección base de la vector table (VBAR). Esto le dice al procesador que use nuestra vector table personalizada en lugar de la dirección que trae por defecto.

3) Por qué si `main` retorna el código entra en un ciclo infinito?

- Me surgió la duda porque el código base contenía este comentario: `// If main returns, loop forever`. Esto se debe a que en este tipo de sistemas (bare-metal), `main` generalmente no retorna. Si `main` llegara a retornar, el programa continuaría ejecutando las instrucciones que siguen al `bl main`, que en este caso es un ciclo infinito (`hang`). Esto lo hacen como medida de seguridad para evitar que el programa ejecute código no definido o se comporte de manera impredecible si `main` retorna.

4) Por qué restar 4 a PC (`subs pc, lr, #4`)?

- Debido al pipeline de tres etapas (Fetch-Decode-Execute), el PC siempre apunta dos instrucciones por delante de la que se está ejecutando. Cuando ocurre una interrupción, el hardware guarda el valor actual del PC en el LR, pero, este valor es Dirección de Ejecución + 8. Para reanudar el programa exactamente en la siguiente instrucción pendiente (Dirección de Ejecución + 4), debemos restar 4 bytes al valor almacenado en el LR.

5) Cuál es la diferencia entre CPSR y SPSR?

- El CPSR (Current Program Status Register) es el registro de estado activo que contiene los flags de condición, el modo actual del CPU y el estado de las interrupciones en tiempo real. En cambio, el SPSR (Saved Program Status Register) actúa como una "fotografía" de seguridad que cuando ocurre una excepction, el hardware copia automáticamente el CPSR al SPSR del modo correspondiente para preservar el estado del programa principal antes del salto al handler.


6) `68 - 64` en `PUT32(INTC_MIR_CLEAR2, 1 << (68 - 64));`

- `INTC_MIR_CLEAR2` controla los IRQs del 64 al 95. Dentro de ese registro de 32 bits, el bit 0 corresponde al IRQ 64, el bit 1 al IRQ 65, y así sucesivamente. Para apuntar al IRQ 68, se calcula el índice del bit dentro del banco 2 como `(68 - 64) = 4`, luego se escribe `1 << 4` para limpiar la máscara para ese IRQ específico.


7) Problema de “doble arranque” después del primer `Tick`

- El sistema imprimía correctamente:
  - `Starting OS...`
  - `Initializing timer...`
  - `Enabling interrupts...`
  - `Interrupts enabled. Starting main loop...`
  luego llegaba el primer `Tick` y vuelve a imprimir esos mensajes como si reiniciara desde cero.

  - Una de las causas era que el `irq_handler` hace `push/pop` de registros, pero al entrar a una IRQ no se usa el stack normal de `main`, sino el stack del modo IRQ.
  - Si `SP_irq` no está inicializado, el `push` en la IRQ escribe en una dirección inválida/inesperada y corrompe estado y el flujo termina reejecutando el arranque.

  - Se agregó una región de stack dedicada para IRQ en `.bss`:
    - `_irq_stack_bottom`
    - `_irq_stack_top`
