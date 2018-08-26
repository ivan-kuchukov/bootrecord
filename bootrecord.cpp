#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
using namespace std;

char* partitionType[0xFF];
bool extendedPartitionType[0xFF];
long extendedPartitionFirstSector;
long currentPartitionFirstSector;
int partitionNumber=0;
int blockSize=0;
bool showCHS=false;
bool onlyMBR=false;
char* unit[5] {"B","KiB","MiB","GiB","TiB"};
string filename="";

// get block size of disk from sysfile
int getBlockSize() {
	string line;
	int blockSize;	
	ifstream file ("/sys/block/"+filename.substr(filename.find_last_of("/")+1,filename.length())+"/queue/physical_block_size");
	if (file.is_open())	{
		getline (file,line);
		file.close();
		stringstream stream(line);
		stream >> blockSize;
		return blockSize;
	} else {
		return 0;
	}
}

// show bytes as Hex String
string showHex(char* &memblock, int start, int length) {
	ostringstream s;
	s << " [";
	for (int i=start; i<start+length; i++) {
		s << setfill('0') << setw(2) << hex << (int)(unsigned char)memblock[i];
	}
	s << "] ";
	return s.str();
}

// read sector of boot record
int readBootRecord(ifstream &file, unsigned long sector)
{
	unsigned long partitionFirstSector;
	streampos size=512;
	char* memblock;
	memblock = new char [size];	
	unsigned long position=sector*blockSize;
	currentPartitionFirstSector=0;

	file.seekg (position);
	file.read (memblock, size);
	
	string words;
	string word ("");
	for (int i = 0; i < 446; i++) {
		if ( ((memblock[i]>='A') and (memblock[i]<='Z'))
			or ((memblock[i]>='a') and (memblock[i]<='z')) )
		{
			word = word + memblock[i];
		} else {
			if (word.length() > 3) {
				words = words + word + " ";
			}
			word = "";
		}
	}
	if ( words.length()!=0 ) {
		cout << "Words in bootstrap code area: " << words << endl;		
	}
	
	if ( (unsigned char)memblock[252]==0xaa and (unsigned char)memblock[253]==0x55 ) {
		cout << "Disk has Disk Manager signature [aa55] and may have 16 partitions, but the program show only 4 main partitions." << endl;
	}
	if ( (unsigned char)memblock[380]==0x5a and (unsigned char)memblock[381]==0xa5 ) {
		cout << "Disk has AST/NEC signature [5aa5] and may have 8 partitions, but the program show only 4 main partitions." << endl;
	}
	if ( (unsigned char)memblock[428]==0x78 and (unsigned char)memblock[429]==0x56 ) {
		cout << "Disk has AAP signature [7856]." << endl;
	}
	if ( memblock[2]=='N' and memblock[3]=='E' and memblock[4]=='W' and memblock[5]=='L'
		 and memblock[6]=='D' and memblock[7]=='R' ) {
		cout << "Disk has NEWLDR signature [NEWLDR]." << endl;
	}
	if ( (unsigned char)memblock[218]==0x00 and (unsigned char)memblock[219]==0x00 
		and (unsigned char)memblock[220]>=0x80 
		and (
			( (unsigned char)memblock[444]==0x00 and (unsigned char)memblock[445]==0x00 )
			or
			( (unsigned char)memblock[444]==0x5a and (unsigned char)memblock[445]==0x5a )
			)
		)
	{
		cout << "Disk timestamp: " << (int)(unsigned char)memblock[223] << ":" << (int)(unsigned char)memblock[222] << ":" << (int)(unsigned char)memblock[221];
		cout << showHex(memblock,221,3) << endl;
		cout << "Disk signature:" << showHex(memblock,440,4) << endl;
		if ((unsigned char)memblock[444]==0x5a ) {
			cout << "Disk has copy-protected mark [5a5a]." << endl;
		}
	}

	for (int i = 446; i < 510; i = i + 16) {	
		if (sector!=0 and i>462) {
			//last 2 partitions in EBR must be empty
			if ((int)(unsigned char)memblock[i]!=0) {
				cout << "ERROR! EBR partition is not empty." << endl;
			}
		} else {
			cout << endl;
			cout << "Type: ";
			cout << partitionType[(unsigned char)memblock[i+4]];
			cout << showHex(memblock,i+4,1) << endl;
			cout << "Boot status: ";
			if ((unsigned char)memblock[i]==0x00) {
				cout << "Inactive";
			} else if ((unsigned char)memblock[i]==0x80) {
				cout << "Active";
			} else {
				cout << "ERROR! Incorrect value";
			}
			cout << showHex(memblock,i,1) << endl;
			if (showCHS) {
				cout << "First sector (CHS):";
				cout << showHex(memblock,i+1,3) << endl;
				cout << "Last sector (CHS):";
				cout << showHex(memblock,i+5,3) << endl;
			}
			cout << "First sector (LBA): ";
			partitionFirstSector = (unsigned char)memblock[i+8]+(unsigned char)memblock[i+9]*256+(unsigned char)memblock[i+10]*256*256+(unsigned char)memblock[i+11]*256*256*256;
			cout << partitionFirstSector;
			if (extendedPartitionType[(unsigned char)memblock[i+4]]) {
				if (currentPartitionFirstSector!=0) {
					cout << endl << "ERROR! Boot record has more one extended partition" << endl;
					return 1;
				}
				if (sector==0) {
					extendedPartitionFirstSector = partitionFirstSector;
					currentPartitionFirstSector = extendedPartitionFirstSector;
				} else {
					currentPartitionFirstSector = extendedPartitionFirstSector + partitionFirstSector;
				}
			}
			cout << showHex(memblock,i+8,4) << endl;
			cout << "Number of sectors: ";
			long sectorsNumber = (unsigned char)memblock[i+12]+(unsigned char)memblock[i+13]*256+(unsigned char)memblock[i+14]*256*256+(unsigned char)memblock[i+15]*256*256*256;
			cout << sectorsNumber << showHex(memblock,i+12,4) << endl;
			cout << "Size: ";
			long partitionSize = sectorsNumber * blockSize;
			int unitNumber = 0;
			while (partitionSize>=1024 and unitNumber<4) {
				partitionSize = partitionSize / 1024;
				unitNumber++;
			}
			cout << partitionSize << " " << unit[unitNumber];
			cout << " (" << sectorsNumber * blockSize << " B)" << endl;
		}								
	}
	
	if (!((unsigned char)memblock[510]==0x55 and (unsigned char)memblock[511]==0xaa)) {
		cout << endl << "ERROR! Incorrect signature" << showHex(memblock,510,2) << endl;
		return 1;
	}

	delete[] memblock;
}

int main (int argc, char* argv[]) {
	if ( argc == 1 ) {
		argv[argc++] = "-h";
	}
	// command-line options:
	for (int i=1; i<argc; ++i) {
		string arg = argv[i];
		arg=arg.substr(0,arg.find_first_of("="));
		string value = argv[i];
		if(arg.length()<value.length()) {
			value=value.substr(arg.length()+1,value.length()-arg.length());
		} else {
			value="";
		}
		if ( arg == "-h" or arg == "--help" or argc == 1 ) {
			cout << endl;  
			cout << "Usage: bootrecord <disk> [OPTIONS]" << endl;
			cout << endl;
			cout << "Display information about <disk> boot record (MBR and EBR)" << endl;
			cout << endl;
			cout << "Options:" << endl;
			cout << "     --block=BYTES      set block size for <disk> (512,4096)" << endl;
			cout << " -c, --chs              display CHS information" << endl;
			cout << " -m, --mbr              display only MBR" << endl;
			cout << " -h, --help             display this help" << endl;
			cout << endl;
			cout << "Examples:" << endl;
			cout << " bootrecord /dev/sda" << endl;
			cout << " bootrecord --block=512 file" << endl;
			cout << endl;
			return 0;
		}
		else if ( arg == "--block" ) {
			stringstream stream(value);
			stream >> blockSize;
		}
		else if ( arg == "-c" or arg == "--chs" ) {
			showCHS=true;
		}
		else if ( arg == "-m" or arg == "--mbr" ) {
			onlyMBR=true;
		}
		else if ( arg.substr(0,1) == "-" ) {
			cout << "Unknown option: " << arg << endl;
			return 1;
		}
		else {
			filename=arg;
		}
	}
	// init partitionType
	for (int i=0x00; i<=0xFF; i++) {
		partitionType[i]="";
	}
	// ToDO: load values of partition types from file
	{
	partitionType[0x00]="Empty";
	partitionType[0x05]="Extended partition (CHS)";
	partitionType[0x07]="NTFS/exFAT";
	partitionType[0x06]="FAT16B";
	partitionType[0x0B]="FAT32 (CHS)";
	partitionType[0x0C]="FAT32 (LBA)";
	partitionType[0x0F]="Extended partition (LBA)";
	partitionType[0x27]="Windows Recovery Environment (RE)";
	partitionType[0x42]="Dynamic extended partition marker";
	partitionType[0x82]="Linux swap space";
	partitionType[0x83]="Linux";
	partitionType[0x85]="Linux extended";
	partitionType[0x88]="Linux plaintext partition table";
	partitionType[0x8E]="Linux LVM";
	partitionType[0xEE]="GPT protective MBR";
	partitionType[0xEF]="EFI system partition";
	partitionType[0xFD]="Linux RAID superblock with auto-detect";
	partitionType[0xFE]="Disk Administration hidden partition";	 
	extendedPartitionType[0x05]=true;
	extendedPartitionType[0x0F]=true;
	extendedPartitionType[0x15]=true;
	extendedPartitionType[0x1F]=true;
	extendedPartitionType[0x85]=true;
	extendedPartitionType[0x91]=true;
	extendedPartitionType[0x9B]=true;
	extendedPartitionType[0xC5]=true;
	extendedPartitionType[0xCF]=true;
	extendedPartitionType[0xD5]=true;
	}

	ifstream file (filename, ios::in|ios::binary|ios::ate);
	if (file.is_open())	{
		if (blockSize==0) {
			blockSize = getBlockSize();
		}
		if (blockSize==0 or blockSize>1024*1024) {
			cout << "Can't identify block size for your data. You can set it by manual, use --help for list of options" << endl; 
			return 1;
		}
		cout << "Block size:" << blockSize << endl << endl;
		cout << "*** MBR:" << endl;
		readBootRecord(file,0);
		if (onlyMBR) {
			cout << endl;
			return 0;
		}
		while(currentPartitionFirstSector!=0) {			
			cout << endl << "*** EBR. Absolute position: " << currentPartitionFirstSector << endl;
			readBootRecord(file,currentPartitionFirstSector);
		}
		
		file.close();
	}
	else {
		cout << "Unable to read";
	}
    cout << endl;    
	return 0;
}
