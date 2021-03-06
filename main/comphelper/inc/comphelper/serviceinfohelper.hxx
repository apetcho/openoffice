/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



#ifndef COMPHELPER_SERVICEINFOHELPER_HXX
#define COMPHELPER_SERVICEINFOHELPER_HXX

#include <com/sun/star/lang/XServiceInfo.hpp>
#include "comphelper/comphelperdllapi.h"

namespace comphelper {

/** this class provides a basic helper for classes suporting the XServiceInfo Interface.
 *
 *  you can overload the <code>getSupprotedServiceNames</code> to implement a XServiceInfo.
 *  you can use the static helper methods to combine your services with that of parent
 *  or aggregatet classes.
 */
class COMPHELPER_DLLPUBLIC ServiceInfoHelper : public ::com::sun::star::lang::XServiceInfo
{
public:
	// XServiceInfo
    virtual ::rtl::OUString SAL_CALL getImplementationName() throw(::com::sun::star::uno::RuntimeException);
    virtual sal_Bool SAL_CALL supportsService( const ::rtl::OUString& ServiceName ) throw(::com::sun::star::uno::RuntimeException);
    virtual ::com::sun::star::uno::Sequence< ::rtl::OUString > SAL_CALL getSupportedServiceNames() throw(::com::sun::star::uno::RuntimeException);

	// helper
	static ::com::sun::star::uno::Sequence< ::rtl::OUString > concatSequences( const ::com::sun::star::uno::Sequence< ::rtl::OUString >& rSeq1,
			const ::com::sun::star::uno::Sequence< ::rtl::OUString >& rSeq2 ) throw();
	static void addToSequence( ::com::sun::star::uno::Sequence< ::rtl::OUString >& rSeq, sal_uInt16 nServices, /* sal_Char* */... ) throw();
	static sal_Bool SAL_CALL supportsService( const ::rtl::OUString& ServiceName, const ::com::sun::star::uno::Sequence< ::rtl::OUString >& SupportedServices ) throw();
};

}

#endif

