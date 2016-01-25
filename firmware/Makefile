
TARGET=msp
TARGET=stellaris
TARGET=tiva

include ./Makefile-${TARGET}

ifeq (${TARGET}, tiva)
PART=TM4C123GH6PM
ROOT=../tiva-c
VPATH=./tiva/
VPATH+=${ROOT}/utils
MAKEDEFS=${ROOT}/makedefs
IPATH=${ROOT}
IPATH+=./${TARGET}
else 
ifeq (${TARGET}, stellaris)
PART=LM4F120H5QR
VARIANT=cm4f
ROOT=../stellaris
VPATH=./stellaris/
VPATH+=${ROOT}/utils
MAKEDEFS=${ROOT}/makedefs
IPATH=${ROOT}
IPATH+=./${TARGET}
else 
ifeq (${TARGET}, msp)
else 
$(error Unknown target ${TARGET}!)
endif
endif
endif

IPATH+=./inc

include ${MAKEDEFS}

CFLAGS+=--std=c11
LD=${PREFIX}-gcc
LDFLAGS=
LDFLAGS+=-Wl,--gc-sections 
#CFLAGS+=-flto
#LDFLAGS+=-flto 


all: ${COMPILER}
all: ${COMPILER}/main.axf

#
# The rule to clean out all the build products.
#
clean:
	@rm -rf ${COMPILER} ${wildcard *~}

#
# The rule to create the target directory.
#
${COMPILER}:
	@mkdir -p ${COMPILER}

${COMPILER}/main.axf: ${COMPILER}/main.o
${COMPILER}/main.axf: ${COMPILER}/config.o
${COMPILER}/main.axf: ${COMPILER}/debug.o
${COMPILER}/main.axf: ${COMPILER}/spi.o
${COMPILER}/main.axf: ${COMPILER}/led.o
${COMPILER}/main.axf: ${COMPILER}/usb.o
${COMPILER}/main.axf: ${COMPILER}/tick.o
${COMPILER}/main.axf: ${COMPILER}/proto.o

${COMPILER}/main.axf: ${COMPILER}/startup_${COMPILER}.o
${COMPILER}/main.axf: ${COMPILER}/usb_bulk_structs.o
${COMPILER}/main.axf: ${COMPILER}/ustdlib.o
${COMPILER}/main.axf: ${COMPILER}/uartstdio.o
${COMPILER}/main.axf: ${TARGET}/usb_dev_bulk.ld

#
# Rules for building the USB device bulk example.
#
ifeq (${TARGET}, tiva)
${COMPILER}/main.axf: ${ROOT}/usblib/${COMPILER}/libusb.a
${COMPILER}/main.axf: ${ROOT}/driverlib/${COMPILER}/libdriver.a
SCATTERgcc_main=${TARGET}/usb_dev_bulk.ld
ENTRY_main=ResetISR
CFLAGSgcc=-DTARGET_IS_BLIZZARD_RA1 -DUART_BUFFERED
else 
ifeq (${TARGET}, stellaris)
${COMPILER}/main.axf: ${ROOT}/usblib/${COMPILER}-cm4f/libusb-cm4f.a
${COMPILER}/main.axf: ${ROOT}/driverlib/${COMPILER}-cm4f/libdriver-cm4f.a
SCATTERgcc_main=${TARGET}/usb_dev_bulk.ld
ENTRY_main=ResetISR
CFLAGSgcc=-DTARGET_IS_BLIZZARD_RA1 -DUART_BUFFERED
else 
ifeq (${TARGET}, msp)
else 
$(error Unknown target ${TARGET}!)
endif
endif
endif

#
# Include the automatically generated dependency files.
#
ifneq (${MAKECMDGOALS},clean)
-include ${wildcard ${COMPILER}/*.d} __dummy__
endif

