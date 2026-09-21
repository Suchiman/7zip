// Qt/PropertyNameTable.h
//
// PROPID -> column caption, generated from
//   UI/FileManager/PropertyNameRes.h + UI/FileManager/PropertyName.rc
// (the resource id of a property name is 1000 + PROPID, see
//  UI/FileManager/PropertyName.cpp::GetNameOfProperty).
// Regenerate with tools/gen_prop_names.sh after updating the 7-Zip sources.

#ifndef ZIP7_INC_QT_PROPERTY_NAME_TABLE_H
#define ZIP7_INC_QT_PROPERTY_NAME_TABLE_H

struct CPropNamePair
{
  unsigned PropID;
  const char *Name;
};

static const CPropNamePair g_PropNames[] =
{
  { 3, "Path" }, // IDS_PROP_PATH
  { 4, "Name" }, // IDS_PROP_NAME
  { 5, "Extension" }, // IDS_PROP_EXTENSION
  { 6, "Folder" }, // IDS_PROP_IS_FOLDER
  { 7, "Size" }, // IDS_PROP_SIZE
  { 8, "Packed Size" }, // IDS_PROP_PACKED_SIZE
  { 9, "Attributes" }, // IDS_PROP_ATTRIBUTES
  { 10, "Created" }, // IDS_PROP_CTIME
  { 11, "Accessed" }, // IDS_PROP_ATIME
  { 12, "Modified" }, // IDS_PROP_MTIME
  { 13, "Solid" }, // IDS_PROP_SOLID
  { 14, "Commented" }, // IDS_PROP_C0MMENTED
  { 15, "Encrypted" }, // IDS_PROP_ENCRYPTED
  { 16, "Split Before" }, // IDS_PROP_SPLIT_BEFORE
  { 17, "Split After" }, // IDS_PROP_SPLIT_AFTER
  { 18, "Dictionary" }, // IDS_PROP_DICTIONARY_SIZE
  { 19, "CRC" }, // IDS_PROP_CRC
  { 20, "Type" }, // IDS_PROP_FILE_TYPE
  { 21, "Anti" }, // IDS_PROP_ANTI
  { 22, "Method" }, // IDS_PROP_METHOD
  { 23, "Host OS" }, // IDS_PROP_HOST_OS
  { 24, "File System" }, // IDS_PROP_FILE_SYSTEM
  { 25, "User" }, // IDS_PROP_USER
  { 26, "Group" }, // IDS_PROP_GROUP
  { 27, "Block" }, // IDS_PROP_BLOCK
  { 28, "Comment" }, // IDS_PROP_COMMENT
  { 29, "Position" }, // IDS_PROP_POSITION
  { 30, "Path Prefix" }, // IDS_PROP_PREFIX
  { 31, "Folders" }, // IDS_PROP_FOLDERS
  { 32, "Files" }, // IDS_PROP_FILES
  { 33, "Version" }, // IDS_PROP_VERSION
  { 34, "Volume" }, // IDS_PROP_VOLUME
  { 35, "Multivolume" }, // IDS_PROP_IS_VOLUME
  { 36, "Offset" }, // IDS_PROP_OFFSET
  { 37, "Links" }, // IDS_PROP_LINKS
  { 38, "Blocks" }, // IDS_PROP_NUM_BLOCKS
  { 39, "Volumes" }, // IDS_PROP_NUM_VOLUMES
  { 41, "64-bit" }, // IDS_PROP_BIT64
  { 42, "Big-endian" }, // IDS_PROP_BIG_ENDIAN
  { 43, "CPU" }, // IDS_PROP_CPU
  { 44, "Physical Size" }, // IDS_PROP_PHY_SIZE
  { 45, "Headers Size" }, // IDS_PROP_HEADERS_SIZE
  { 46, "Checksum" }, // IDS_PROP_CHECKSUM
  { 47, "Characteristics" }, // IDS_PROP_CHARACTS
  { 48, "Virtual Address" }, // IDS_PROP_VA
  { 49, "ID" }, // IDS_PROP_ID
  { 50, "Short Name" }, // IDS_PROP_SHORT_NAME
  { 51, "Creator Application" }, // IDS_PROP_CREATOR_APP
  { 52, "Sector Size" }, // IDS_PROP_SECTOR_SIZE
  { 53, "Mode" }, // IDS_PROP_POSIX_ATTRIB
  { 54, "Symbolic Link" }, // IDS_PROP_SYM_LINK
  { 55, "Error" }, // IDS_PROP_ERROR
  { 56, "Total Size" }, // IDS_PROP_TOTAL_SIZE
  { 57, "Free Space" }, // IDS_PROP_FREE_SPACE
  { 58, "Cluster Size" }, // IDS_PROP_CLUSTER_SIZE
  { 59, "Label" }, // IDS_PROP_VOLUME_NAME
  { 60, "Local Name" }, // IDS_PROP_LOCAL_NAME
  { 61, "Provider" }, // IDS_PROP_PROVIDER
  { 62, "NT Security" }, // IDS_PROP_NT_SECURITY
  { 63, "Alternate Stream" }, // IDS_PROP_ALT_STREAM
  { 64, "Aux" }, // IDS_PROP_AUX
  { 65, "Deleted" }, // IDS_PROP_DELETED
  { 66, "Is Tree" }, // IDS_PROP_IS_TREE
  { 67, "SHA-1" }, // IDS_PROP_SHA1
  { 68, "SHA-256" }, // IDS_PROP_SHA256
  { 69, "Error Type" }, // IDS_PROP_ERROR_TYPE
  { 70, "Errors" }, // IDS_PROP_NUM_ERRORS
  { 71, "Errors" }, // IDS_PROP_ERROR_FLAGS
  { 72, "Warnings" }, // IDS_PROP_WARNING_FLAGS
  { 73, "Warning" }, // IDS_PROP_WARNING
  { 74, "Streams" }, // IDS_PROP_NUM_STREAMS
  { 75, "Alternate Streams" }, // IDS_PROP_NUM_ALT_STREAMS
  { 76, "Alternate Streams Size" }, // IDS_PROP_ALT_STREAMS_SIZE
  { 77, "Virtual Size" }, // IDS_PROP_VIRTUAL_SIZE
  { 78, "Unpack Size" }, // IDS_PROP_UNPACK_SIZE
  { 79, "Total Physical Size" }, // IDS_PROP_TOTAL_PHY_SIZE
  { 80, "Volume Index" }, // IDS_PROP_VOLUME_INDEX
  { 81, "SubType" }, // IDS_PROP_SUBTYPE
  { 82, "Short Comment" }, // IDS_PROP_SHORT_COMMENT
  { 83, "Code Page" }, // IDS_PROP_CODE_PAGE
  { 84, "Is not archive type" }, // IDS_PROP_IS_NOT_ARC_TYPE
  { 85, "Physical Size can't be detected" }, // IDS_PROP_PHY_SIZE_CANT_BE_DETECTED
  { 86, "Zeros Tail Is Allowed" }, // IDS_PROP_ZEROS_TAIL_IS_ALLOWED
  { 87, "Tail Size" }, // IDS_PROP_TAIL_SIZE
  { 88, "Embedded Stub Size" }, // IDS_PROP_EMB_STUB_SIZE
  { 89, "Link" }, // IDS_PROP_NT_REPARSE
  { 90, "Hard Link" }, // IDS_PROP_HARD_LINK
  { 91, "iNode" }, // IDS_PROP_INODE
  { 92, "Stream ID" }, // IDS_PROP_STREAM_ID
  { 93, "Read-only" }, // IDS_PROP_READ_ONLY
  { 94, "Out Name" }, // IDS_PROP_OUT_NAME
  { 95, "Copy Link" }, // IDS_PROP_COPY_LINK
  { 96, "ArcFileName" }, // IDS_PROP_ARC_FILE_NAME
  { 97, "IsHash" }, // IDS_PROP_IS_HASH
  { 98, "Metadata Changed" }, // IDS_PROP_CHANGE_TIME
  { 99, "User ID" }, // IDS_PROP_USER_ID
  { 100, "Group ID" }, // IDS_PROP_GROUP_ID
  { 101, "Device Major" }, // IDS_PROP_DEVICE_MAJOR
  { 102, "Device Minor" }, // IDS_PROP_DEVICE_MINOR
  { 103, "Dev Major" }, // IDS_PROP_DEV_MAJOR
  { 104, "Dev Minor" }, // IDS_PROP_DEV_MINOR
};

#endif
