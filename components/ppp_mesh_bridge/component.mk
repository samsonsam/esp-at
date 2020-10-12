#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)
# I thought this was supposed to happen automatically, but it is not always working:
COMPONENT_INCLUDES += $(COMPONENT_PATH)/include
