# ControlSerialLeds

STM32F401CCU6 (HSI 16 MHz) con timer, botón por interrupción y comandos por puerto serial.

| Señal | Pin | Función |
|---|---|---|
| LATIDO | PB0 | Parpadea cada 300 ms en el lazo principal (sin `HAL_Delay`) |
| TIMER_RAPIDO | PB1 | Cambia cada 250 ms (interrupción de **TIM3**) |
| TIMER_LENTO | PB10 | Cambia cada 2 s (cada 8 interrupciones de TIM3) |
| PULSADOR | PA0 | Entrada EXTI0, flanco de bajada, pull-up interno, antirrebote 150 ms |
| LUZ_PULSADOR | PB5 | Se invierte con cada pulsación |
| LUZ_SERIAL | PB8 | Se controla por comandos seriales |
| USART2_TX / RX | PA2 / PA3 | 115200 baudios 8N1, recepción por **DMA1 Stream5 + IDLE** |

## Comandos (no importa si van en mayúsculas o minúsculas)
| Comando | Acción |
|---|---|
| `ENCENDER` | Prende LUZ_SERIAL |
| `APAGAR` | Apaga LUZ_SERIAL |
| `CAMBIAR` | Invierte LUZ_SERIAL |
| `ESTADO` | Responde el estado de la luz y el número de pulsaciones |
| `AYUDA` | Lista los comandos |

Los comandos están en una tabla (`tablaComandos[]`): cada comando apunta a su función.
La recepción usa la librería del curso `UARTRX` (Core/Librerias), configurada para `huart2` / `USART2`.

## Proteus
- PB0, PB1, PB10, PB5, PB8 → 220 Ω → LED → GND.
- Pulsador entre PA0 y GND (el pull-up es interno).
- Virtual Terminal: RXD ← PA2, TXD → PA3, a 115200 baudios.
