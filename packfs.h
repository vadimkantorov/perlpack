int packfsfilesnum = 0, packfsdirsnum = 0;

// extern char _binary_{filepathwithunderscoresaspathsep}_start[], _binary_{filepathwithunderscoresaspathsep}_end[]; 

struct packfsinfo { const char* safe_path; const char *path; const char* start; const char* end; } packfsinfos[] = {};

const char* packfsdirs[] = {};
