#
# Copyright 2014, Broadcom Corporation
# All Rights Reserved.
#
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#

NAME := App_Smart_strip

$(NAME)_SOURCES := smart_strip.c \
				   power_strip.c \
				   smart_strip_dct.c
                      
GLOBAL_DEFINES :=

WIFI_CONFIG_DCT_H := wifi_config_dct.h

APPLICATION_DCT := smart_strip_dct.c
