#*************************************************************************
#
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
# 
# Copyright 2000, 2011 Oracle and/or its affiliates.
#
# OpenOffice.org - a multi-platform office productivity suite
#
# This file is part of OpenOffice.org.
#
# OpenOffice.org is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License version 3
# only, as published by the Free Software Foundation.
#
# OpenOffice.org is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License version 3 for more details
# (a copy is included in the LICENSE file that accompanied this code).
#
# You should have received a copy of the GNU Lesser General Public License
# version 3 along with OpenOffice.org.  If not, see
# <http://www.openoffice.org/license.html>
# for a copy of the LGPLv3 License.
#
#*************************************************************************

$(eval $(call gb_GoogleTest_GoogleTest,basebmp_test))

$(eval $(call gb_GoogleTest_add_exception_objects,basebmp_test, \
	basebmp/test/basictest \
	basebmp/test/bmpmasktest \
	basebmp/test/bmptest \
	basebmp/test/cliptest \
	basebmp/test/filltest \
	basebmp/test/linetest \
	basebmp/test/main \
	basebmp/test/masktest \
	basebmp/test/polytest \
	basebmp/test/tools \
))

# TODO
# SunStudio 12 (-m64 and -m32 modes): three test cases of the unit tests fail
# if compiled with default -xalias_level (and optimization level -xO3)
#.IF "$(OS)"=="SOLARIS"
# For Sun Studio 8 this switch does not work: compilation fails on bitmapdevice.cxx
#.IF "$(CCNUMVER)"!="00050005"
#CDEFS+=-xalias_level=compatible
#.ENDIF
#.ENDIF

$(eval $(call gb_GoogleTest_add_linked_libs,basebmp_test, \
	basebmp \
	sal \
	stl \
	basegfx \
    $(gb_STDLIBS) \
))

$(eval $(call gb_GoogleTest_add_api,basebmp_test,\
	udkapi \
	offapi \
))

$(eval $(call gb_GoogleTest_set_include,basebmp_test,\
	$$(INCLUDE) \
))

# vim: set noet sw=4 ts=4:
