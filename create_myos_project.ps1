# ======================================================
# Script tạo cấu trúc MyOS (Industrial Layout)
# Chạy trong PowerShell (Windows)
# ======================================================

# Root project name (có thể đổi nếu muốn)
$projectRoot = "MyOS"
New-Item -ItemType Directory -Path $projectRoot -Force | Out-Null
Set-Location $projectRoot

# ======================================================
# 1️⃣ DANH SÁCH THƯ MỤC
# ======================================================

$folders = @(

    # Architecture Layer
    "arch/arm/cortex_m3",

    # Board Support Package
    "bsp/lm3s6965",

    # Kernel Core
    "kernel/core",
    "kernel/debug",
    "kernel/init",

    # OS Components
    "components/ipc",
    "components/sync",
    "components/memory",
    "components/timer",
    "components/algo",

    # Drivers
    "drivers/gpio",
    "drivers/uart",
    "drivers/i2c",
    "drivers/dma",
    "drivers/timer",

    # HAL
    "hal",

    # Application
    "app/tasks",
    "app/services",

    # Config
    "config",

    # Include (Public Headers)
    "include",

    # Build output
    "build",

    # Scripts
    "scripts"
)

foreach ($f in $folders) {
    New-Item -ItemType Directory -Path $f -Force | Out-Null
}

# ======================================================
# 2️⃣ DANH SÁCH FILE SKELETON
# ======================================================

$files = @(

    # Build System
    "Makefile",
    "myos.config",

    # ================= ARCH =================
    "arch/arm/cortex_m3/startup.s",
    "arch/arm/cortex_m3/context_switch.s",
    "arch/arm/cortex_m3/exception.c",
    "arch/arm/cortex_m3/nvic.c",
    "arch/arm/cortex_m3/mpu.c",
    "arch/arm/cortex_m3/port.c",

    # ================= BSP =================
    "bsp/lm3s6965/board_init.c",
    "bsp/lm3s6965/clock.c",
    "bsp/lm3s6965/pinmux.c",
    "bsp/lm3s6965/linker.ld",
    "bsp/lm3s6965/board_config.h",

    # ================= KERNEL =================
    "kernel/core/kernel.c",
    "kernel/core/scheduler.c",
    "kernel/core/task.c",
    "kernel/core/syscall.c",

    "kernel/debug/assert.c",
    "kernel/debug/panic.c",

    "kernel/init/kernel_init.c",

    # ================= COMPONENTS =================
    "components/ipc/queue.c",
    "components/ipc/mailbox.c",
    "components/ipc/event.c",

    "components/sync/mutex.c",
    "components/sync/semaphore.c",

    "components/memory/heap.c",
    "components/memory/mem_pool.c",

    "components/timer/systick.c",
    "components/timer/soft_timer.c",

    "components/algo/banker.c",

    # ================= DRIVERS =================
    "drivers/gpio/gpio_lm3s.c",
    "drivers/uart/uart_lm3s.c",
    "drivers/i2c/i2c_lm3s.c",
    "drivers/dma/dma_lm3s.c",
    "drivers/timer/timer_lm3s.c",

    # ================= HAL =================
    "hal/hal_gpio.c",
    "hal/hal_uart.c",
    "hal/hal_i2c.c",
    "hal/hal_dma.c",
    "hal/hal_timer.c",

    # ================= APPLICATION =================
    "app/main.c",
    "app/tasks/task_sensor.c",
    "app/tasks/task_shell.c",
    "app/tasks/task_comm.c",
    "app/services/logging_service.c",

    # ================= CONFIG =================
    "config/os_config.h",
    "config/memory_map.h",

    # ================= INCLUDE =================
    "include/myos.h",
    "include/os_types.h",
    "include/kernel.h",
    "include/task.h",
    "include/scheduler.h",
    "include/ipc.h",
    "include/mutex.h",
    "include/semaphore.h",
    "include/heap.h",
    "include/hal.h",
    "include/uart.h",
    "include/gpio.h",

    # ================= SCRIPTS =================
    "scripts/build.ps1",
    "scripts/flash.ps1",
    "scripts/debug.ps1"
)

foreach ($f in $files) {
    New-Item -ItemType File -Path $f -Force | Out-Null
}

Write-Host "======================================================="
Write-Host "MyOS industrial folder structure created successfully!"
Write-Host "======================================================="
