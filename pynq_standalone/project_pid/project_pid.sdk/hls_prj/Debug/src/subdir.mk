################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
LD_SRCS += \
../src/lscript.ld 

C_SRCS += \
../src/helloworld.c \
../src/hls_driver.c \
../src/my_mac_driver.c \
../src/platform.c \
../src/xpid_regulator.c 

OBJS += \
./src/helloworld.o \
./src/hls_driver.o \
./src/my_mac_driver.o \
./src/platform.o \
./src/xpid_regulator.o 

C_DEPS += \
./src/helloworld.d \
./src/hls_driver.d \
./src/my_mac_driver.d \
./src/platform.d \
./src/xpid_regulator.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: ARM v7 gcc compiler'
	arm-none-eabi-gcc -Wall -O0 -g3 -c -fmessage-length=0 -MT"$@" -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=hard -I../../hls_prj_bsp/ps7_cortexa9_0/include -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


