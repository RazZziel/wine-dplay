/*
 * Copyright 2008 Juan Lang
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */
#ifndef __CRYPTUIRES_H_
#define __CRYPTUIRES_H_

#define IDS_CERTIFICATE 1000
#define IDS_CERTIFICATEINFORMATION 1001
#define IDS_CERT_INFO_BAD_SIG 1002
#define IDS_CERT_INFO_UNTRUSTED_CA 1003
#define IDS_CERT_INFO_UNTRUSTED_ROOT 1004
#define IDS_CERT_INFO_PARTIAL_CHAIN 1005
#define IDS_CERT_INFO_BAD_PURPOSES 1006
#define IDS_CERT_INFO_PURPOSES 1007

#define IDS_SUBJECT_HEADING 1010
#define IDS_ISSUER_HEADING 1011
#define IDS_VALID_FROM 1012
#define IDS_VALID_TO 1013
#define IDS_CERTIFICATE_BAD_SIGNATURE 1014
#define IDS_CERTIFICATE_BAD_TIME 1015
#define IDS_CERTIFICATE_BAD_TIMENEST 1016
#define IDS_CERTIFICATE_REVOKED 1017
#define IDS_CERTIFICATE_VALID 1018
#define IDS_FIELD 1019
#define IDS_VALUE 1020
#define IDS_FIELDS_ALL 1021
#define IDS_FIELDS_V1 1022
#define IDS_FIELDS_EXTENSIONS 1023
#define IDS_FIELDS_CRITICAL_EXTENSIONS 1024
#define IDS_FIELDS_PROPERTIES 1025
#define IDS_FIELD_VERSION 1026
#define IDS_FIELD_SERIAL_NUMBER 1027
#define IDS_FIELD_ISSUER 1028
#define IDS_FIELD_VALID_FROM 1029
#define IDS_FIELD_VALID_TO 1030
#define IDS_FIELD_SUBJECT 1031
#define IDS_FIELD_PUBLIC_KEY 1032
#define IDS_FIELD_PUBLIC_KEY_FORMAT 1033
#define IDS_PROP_HASH 1034
#define IDS_PROP_ENHKEY_USAGE 1035
#define IDS_PROP_FRIENDLY_NAME 1036
#define IDS_PROP_DESCRIPTION 1037
#define IDS_CERTIFICATE_PROPERTIES 1038
#define IDS_CERTIFICATE_PURPOSE_ERROR 1039
#define IDS_CERTIFICATE_PURPOSE_EXISTS 1040
#define IDS_SELECT_STORE_TITLE 1041
#define IDS_SELECT_STORE 1042
#define IDS_IMPORT_WIZARD 1043
#define IDS_IMPORT_TYPE_MISMATCH 1044
#define IDS_IMPORT_FILE_TITLE 1045
#define IDS_IMPORT_FILE_SUBTITLE 1046
#define IDS_IMPORT_STORE_TITLE 1047
#define IDS_IMPORT_STORE_SUBTITLE 1048
#define IDS_IMPORT_FILTER_CERT 1049
#define IDS_IMPORT_FILTER_PFX 1050
#define IDS_IMPORT_FILTER_CRL 1051
#define IDS_IMPORT_FILTER_CTL 1052
#define IDS_IMPORT_FILTER_SERIALIZED_STORE 1053
#define IDS_IMPORT_FILTER_CMS 1054
#define IDS_IMPORT_FILTER_ALL 1055
#define IDS_IMPORT_EMPTY_FILE 1056
#define IDS_IMPORT_BAD_FORMAT 1057
#define IDS_IMPORT_OPEN_FAILED 1058
#define IDS_IMPORT_DEST_DETERMINED 1059
#define IDS_IMPORT_SELECT_STORE 1060
#define IDS_IMPORT_STORE_SELECTION 1061
#define IDS_IMPORT_DEST_AUTOMATIC 1062
#define IDS_IMPORT_FILE 1063
#define IDS_IMPORT_CONTENT 1064
#define IDS_IMPORT_CONTENT_CERT 1065
#define IDS_IMPORT_CONTENT_CRL 1066
#define IDS_IMPORT_CONTENT_CTL 1067
#define IDS_IMPORT_CONTENT_CMS 1068
#define IDS_IMPORT_CONTENT_PFX 1069
#define IDS_IMPORT_CONTENT_STORE 1070
#define IDS_IMPORT_SUCCEEDED 1071
#define IDS_IMPORT_FAILED 1072
#define IDS_WIZARD_TITLE_FONT 1073

#define IDS_PURPOSE_SERVER_AUTH 1100
#define IDS_PURPOSE_CLIENT_AUTH 1101
#define IDS_PURPOSE_CODE_SIGNING 1102
#define IDS_PURPOSE_EMAIL_PROTECTION 1103
#define IDS_PURPOSE_IPSEC 1104
#define IDS_PURPOSE_TIMESTAMP_SIGNING 1105
#define IDS_PURPOSE_CTL_USAGE_SIGNING 1106
#define IDS_PURPOSE_EFS 1107
#define IDS_PURPOSE_EFS_RECOVERY 1108
#define IDS_PURPOSE_WHQL 1109
#define IDS_PURPOSE_NT5 1110
#define IDS_PURPOSE_OEM_WHQL 1111
#define IDS_PURPOSE_EMBEDDED_NT 1112
#define IDS_PURPOSE_ROOT_LIST_SIGNER 1113
#define IDS_PURPOSE_QUALIFIED_SUBORDINATION 1114
#define IDS_PURPOSE_KEY_RECOVERY 1115
#define IDS_PURPOSE_DOCUMENT_SIGNING 1116
#define IDS_PURPOSE_LIFETIME_SIGNING 1117
#define IDS_PURPOSE_DRM 1118
#define IDS_PURPOSE_LICENSES 1119
#define IDS_PURPOSE_LICENSE_SERVER 1120
#define IDS_PURPOSE_ENROLLMENT_AGENT 1121
#define IDS_PURPOSE_SMARTCARD_LOGON 1122
#define IDS_PURPOSE_CA_EXCHANGE 1123
#define IDS_PURPOSE_KEY_RECOVERY_AGENT 1124
#define IDS_PURPOSE_DS_EMAIL_REPLICATION 1125

#define IDD_GENERAL 100
#define IDD_DETAIL 101
#define IDD_HIERARCHY 102
#define IDD_USERNOTICE 103
#define IDD_CERT_PROPERTIES_GENERAL 104
#define IDD_ADD_CERT_PURPOSE 105
#define IDD_SELECT_STORE 106
#define IDD_IMPORT_WELCOME 107
#define IDD_IMPORT_FILE 108
#define IDD_IMPORT_STORE 109
#define IDD_IMPORT_FINISH 110

#define IDB_SMALL_ICONS 200
#define IDB_CERT 201
#define IDB_CERT_ERROR 202
#define IDB_CERT_WARNING 203
#define IDB_CHECKS 204
#define IDB_CERT_WATERMARK 205
#define IDB_CERT_HEADER 206

#define IDC_STATIC 2000
#define IDC_CERTIFICATE_ICON 2001
#define IDC_CERTIFICATE_INFO 2002
#define IDC_CERTIFICATE_STATUS 2003
#define IDC_CERTIFICATE_NAMES 2004
#define IDC_ADDTOSTORE 2005
#define IDC_ISSUERSTATEMENT 2006

#define IDC_DETAIL_SELECT 2100
#define IDC_DETAIL_LIST 2101
#define IDC_DETAIL_VALUE 2102
#define IDC_EDITPROPERTIES 2103
#define IDC_EXPORT 2104

#define IDC_VIEWCERTIFICATE 2200
#define IDC_CERTPATH 2201
#define IDC_CERTIFICATESTATUS 2202
#define IDC_CERTIFICATESTATUSTEXT 2203

#define IDC_USERNOTICE 2300
#define IDC_CPS 2301

#define IDC_FRIENDLY_NAME 2400
#define IDC_DESCRIPTION 2401
#define IDC_ENABLE_ALL_PURPOSES 2402
#define IDC_DISABLE_ALL_PURPOSES 2403
#define IDC_ENABLE_SELECTED_PURPOSES 2404
#define IDC_CERTIFICATE_USAGES 2405
#define IDC_ADD_PURPOSE 2406

#define IDC_NEW_PURPOSE 2500

#define IDC_STORE_TEXT 2600
#define IDC_STORE_LIST 2601
#define IDC_SHOW_PHYSICAL_STORES 2602

#define IDC_IMPORT_TITLE 2700
#define IDC_IMPORT_FILENAME 2701
#define IDC_IMPORT_BROWSE_FILE 2702
#define IDC_IMPORT_AUTO_STORE 2703
#define IDC_IMPORT_SPECIFY_STORE 2704
#define IDC_IMPORT_STORE 2705
#define IDC_IMPORT_BROWSE_STORE 2706
#define IDC_IMPORT_SETTINGS 2707

#endif /* ndef __CRYPTUIRES_H_ */