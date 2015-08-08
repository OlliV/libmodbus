//------------------------------------------------------------------------------
// Copyright (C) 2014, Robert Johansson, Raditex Control AB
// All rights reserved.
//
// rSCADA 
// http://www.rSCADA.se
// info@rscada.se
//
//------------------------------------------------------------------------------

#include "modbus.h"
#include "modbus-serial.h"

#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>

#include <stdio.h>
#include <strings.h>

#include <termios.h>
#include <errno.h>
#include <string.h>

#define frame_BUFF_SIZE 2048

//#define MODBUS_HEADER_LENGTH + len
static int debug = 0;

//------------------------------------------------------------------------------
/// Set up a serial connection handle.
//------------------------------------------------------------------------------
modbus_serial_handle_t *
modbus_serial_connect(char *device, long baudrate)
{
    modbus_serial_handle_t *handle;

    if (device == NULL)
    {
        return NULL;
    }

    if ((handle = (modbus_serial_handle_t *)malloc(sizeof(modbus_serial_handle_t))) == NULL)
    {
        fprintf(stderr, "%s: failed to allocate memory for handle\n", __PRETTY_FUNCTION__);
        return NULL;
    }

    handle->device = strdup(device);
    
    //
    // create the SERIAL connection
    //

    // Use blocking read and handle it by serial port VMIN/VTIME setting
    if ((handle->fd = open(handle->device, O_RDWR | O_NOCTTY |  O_NDELAY)) < 0)
    {
        fprintf(stderr, "%s: failed to open tty.", __PRETTY_FUNCTION__);
        return NULL;
    }

    bzero(&(handle->t), sizeof(handle->t));
    handle->t.c_cflag |= (CREAD|CLOCAL);
    handle->t.c_cflag |= PARENB;
    handle->t.c_cflag &= ~PARODD;
    handle->t.c_cflag &= ~CSTOPB;
    handle->t.c_cflag &= ~CSIZE;
    handle->t.c_cflag |= CS8;    
    handle->t.c_cc[VMIN]  = 0;
    handle->t.c_cc[VTIME] = 10;

    if (baudrate == 0)
    {
        cfsetispeed(&(handle->t), B9600);
        cfsetospeed(&(handle->t), B9600);
    }
    else
    {
        modbus_serial_set_baudrate(handle, baudrate);   
    }
   
    tcsetattr(handle->fd, TCSANOW, &(handle->t));

    return handle;    
}

//------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------
int
modbus_serial_set_baudrate(modbus_serial_handle_t *handle, long baudrate)
{
    if (handle == NULL)
        return -1;

    switch (baudrate)
    {
        case 300:
            cfsetispeed(&(handle->t), B300);
            cfsetospeed(&(handle->t), B300);
            return 0;

        case 1200:
            cfsetispeed(&(handle->t), B1200);
            cfsetospeed(&(handle->t), B1200);
            return 0;

        case 2400:
            cfsetispeed(&(handle->t), B2400);
            cfsetospeed(&(handle->t), B2400);
            return 0;

        case 9600:
            cfsetispeed(&(handle->t), B9600);
            cfsetospeed(&(handle->t), B9600);
            return 0;

        case 19200:
            cfsetispeed(&(handle->t), B19200);
            cfsetospeed(&(handle->t), B19200);
            return 0;
            
        case 38400:
            cfsetispeed(&(handle->t), B38400);
            cfsetospeed(&(handle->t), B38400);
            return 0;

        case 57600:
            cfsetispeed(&(handle->t), B57600);
            cfsetospeed(&(handle->t), B57600);
            return 0;
                        
       default:
            return -1; // unsupported baudrate
    }
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
int
modbus_serial_disconnect(modbus_serial_handle_t *handle)
{
    if (handle == NULL)
    {
        return -1;
    }

    close(handle->fd);

    if (handle->device)
        free(handle->device);    
        
    free(handle);

    return 0;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
int
modbus_serial_send(modbus_serial_handle_t *handle, modbus_frame_t *pkt)
{
    u_char buff[frame_BUFF_SIZE];
    int len, ret;

    if (handle == 0 || pkt == NULL)
        return -1;

    if ((len = modbus_rtu_frame_pack(pkt, buff, sizeof(buff))) == -1)
    {
        fprintf(stderr, "%s: modbus_frame_t_pack failed\n", __PRETTY_FUNCTION__);
        return -1;
    }
    
    if ((ret = write(handle->fd, buff, len)) != len)
    {   
	    snprintf(modbus_error_str, sizeof(modbus_error_str),
	             "%s: Failed to write frame to serial device (ret = %d: %s)",
	             __PRETTY_FUNCTION__, ret, strerror(errno));
        return -1;
    }

    if (debug)
    {
        int i;
        printf("%s: Wrote %d bytes: ", __PRETTY_FUNCTION__, len);
        for (i = 0; i < len; i++)
        {
            printf("0x%.2x ", buff[i] & 0xFF);
        }
        printf("\n");    
    }

    tcdrain(handle->fd);

    return 0;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
int
modbus_serial_recv(modbus_serial_handle_t *handle, modbus_frame_t *pkt)
{
    u_char buff[frame_BUFF_SIZE];
    int len, ret, i, remaining, nread;
    
    bzero((void *)buff, sizeof(buff));
        
    //
    // first read the MODBUS header
    //
    // You never know how many chars there is on the serial port
    // you must asamble and then parse the the packet...???
    remaining = MODBUS_RTU_HEADER_LENGTH+1;
    nread = 0;

    while (remaining > 0)
    {  
        if ((ret = read(handle->fd, &buff[nread], remaining)) == -1)
        {
            snprintf(modbus_error_str, sizeof(modbus_error_str),
	             "%s: failed to read modbus header from serial device [%d, %d]", __PRETTY_FUNCTION__, ret, nread);
            return -1;
        }
        nread += ret;
        remaining -= ret;
    }

    modbus_rtu_header_parse(pkt, buff, MODBUS_RTU_HEADER_LENGTH);
    
    //
    // compute the length of the remaining data
    //
    switch (pkt->hdr_rtu.func_code)
    {
        case MB_FUNC_READ_COIL_STATUS:
        case MB_FUNC_READ_INPUT_STATUS:
        case MB_FUNC_READ_INPUT_REGISTERS:
        case MB_FUNC_READ_HOLDING_REGISTERS:
            len = 2 + buff[2];        
            break;

        case MB_FUNC_FORCE_SINGLE_COIL:
        case MB_FUNC_PRESET_SINGLE_REGISTER:    
            len = 5;        
            break;   
    
        default:
            len = 8;
    }

    remaining = len;

    while (remaining > 0)
    {
        if ((ret = read(handle->fd, &buff[nread], remaining)) == -1)
        {
            snprintf(modbus_error_str, sizeof(modbus_error_str),
                     "%s: failed to read modbus header from serial device [%d, %d]", __PRETTY_FUNCTION__, ret, nread);
            return -1;
        }
        nread += ret;
        remaining -= ret;
    }


    if (debug)
    {
        printf("%s: Read %d bytes: ", __PRETTY_FUNCTION__, MODBUS_RTU_HEADER_LENGTH + 1 + len);
        for (i = 0; i < MODBUS_RTU_HEADER_LENGTH + 1 + len; i++)
        {
            printf("0x%.2x ", buff[i] & 0xFF);            
        }
        printf("\n");
    }    

    return modbus_rtu_frame_parse(pkt, buff, MODBUS_RTU_HEADER_LENGTH + len);
}


