#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <tss/tss_error.h>
#include <tss/platform.h>
#include <tss/tss_defines.h>
#include <tss/tss_typedef.h>
#include <tss/tss_structs.h>
#include <tss/tspi.h>
#include <trousers/trousers.h>


//#define DEBUG 0
/*#define DBG(message,tResult) if(DEBUG) {fprintf(“(Line %d, %s) %s returned 0x%08x. %s.\n", __LINE__, __func__, message, tResult, trspi_Error_String(tResult));}*/

#define DBG(message, tResult) printf("Line%d, %s)%s returned 0x%08x. %s.\n", __LINE__, __func__, message, tResult, (char *)Trspi_Error_String(tResult))


void extendPCR(TSS_HCONTEXT hContext, int pcrToExtend, BYTE *valueToExtend)
{
	TSS_HTPM hTPM = 0;
	TSS_RESULT result;
	TSS_HKEY hSRK = 0;
	TSS_HPOLICY hSRKPolicy=0;
	TSS_UUID SRK_UUID = TSS_UUID_SRK;
	BYTE wks[20]; 
	memset(wks,0,20);// Place to put the well known secret
	result = Tspi_Context_Create(&hContext);
	result = Tspi_Context_Connect(hContext, NULL);
	// Get the TPM handle
	result=Tspi_Context_GetTpmObject(hContext, &hTPM); 
	//Get the SRK handle
	result=Tspi_Context_LoadKeyByUUID(hContext, TSS_PS_TYPE_SYSTEM, SRK_UUID, &hSRK);
	//Get the SRK policy
	result=Tspi_GetPolicyObject(hSRK, TSS_POLICY_USAGE, &hSRKPolicy);
	//Set the SRK policy to be the well known secret
	result=Tspi_Policy_SetSecret(hSRKPolicy,TSS_SECRET_MODE_SHA1,20, wks); 

	UINT32 PCR_result_length;
	BYTE *Final_PCR_Value;
	result = Tspi_TPM_PcrExtend(hTPM, pcrToExtend, 20, valueToExtend, NULL, &PCR_result_length, &Final_PCR_Value);
	DBG("Extended the PCR", result);

}

void resetPCR(TSS_HCONTEXT hContext, int pcrToReset)
{
	
	TSS_HTPM hTPM = 0;
	TSS_RESULT result;
	TSS_HKEY hSRK = 0;
	TSS_HPOLICY hSRKPolicy=0;
	TSS_UUID SRK_UUID = TSS_UUID_SRK;
	TSS_HPCRS hPcrs;
	result=Tspi_Context_GetTpmObject(hContext, &hTPM);
	result = Tspi_Context_CreateObject(hContext, TSS_OBJECT_TYPE_PCRS, 0, &hPcrs);
	DBG("Reset the PCR", result);
	result = Tspi_PcrComposite_SelectPcrIndex(hPcrs, 23);
	result = Tspi_TPM_PcrReset(hTPM, hPcrs);

	DBG("Reset the PCR", result);

}


void HashThis(TSS_HCONTEXT hContext, BYTE *content, UINT32 contentSize, BYTE hash[20]){
	TSS_RESULT result;
	BYTE *digest;
	UINT32 digestLen;
	TSS_HHASH hHashOfESSKey;
	BYTE initialHash[20];
	memset(initialHash,0,20);
	// Create a generic Hash object
	result=Tspi_Context_CreateObject(hContext, TSS_OBJECT_TYPE_HASH, TSS_HASH_SHA1, &hHashOfESSKey);
	DBG("Create Hash object", result);
	// Hash the data using SHA1
	result=Tspi_Hash_UpdateHashValue(hHashOfESSKey, contentSize, content);
	DBG("Hash in the public key", result);
	result=Tspi_Hash_GetHashValue(hHashOfESSKey, &digestLen, &digest);
	DBG("Get the hashed result", result);
	// so now the digest has the hash of the ESS public key in it
	memcpy(hash,digest,20);
}

long getFileSize(char *filePath){
	long siz = 0;
	FILE *fp = fopen(filePath, "rb");
	if(fp){
		fseek(fp, 0, SEEK_END);
		siz = ftell(fp);	
	}
	fclose(fp);
	return siz;
}


void readFile(char *filePath, long fileSize, BYTE *s){	
	FILE *fp = fopen(filePath, "rb");
	fread(s, sizeof(BYTE), fileSize, fp);
	fclose(fp);
}

int main(int argc, char **argv)
{
	//-----Preamble
	TSS_HCONTEXT hContext=0;
	TSS_HTPM hTPM = 0;
	TSS_RESULT result;
	TSS_HKEY hSRK = 0;
	TSS_HPOLICY hSRKPolicy=0;
	TSS_UUID SRK_UUID = TSS_UUID_SRK;
	TSS_HPCRS hPcrs;
	BYTE wks[20]; // Place to put the well known secret
	memset(wks,0,20); // Set wks to the well known secret of 20 bytes of all zeros
	// Pick the TPM you are talking to in this case the system TPM (which you connect to with “NULL”)
	result = Tspi_Context_Create(&hContext);
	result = Tspi_Context_Connect(hContext, NULL);
	// Get the TPM handle
	result=Tspi_Context_GetTpmObject(hContext, &hTPM); 
	//Get the SRK handle
	result=Tspi_Context_LoadKeyByUUID(hContext, TSS_PS_TYPE_SYSTEM, SRK_UUID, &hSRK);
	//Get the SRK policy
	result=Tspi_GetPolicyObject(hSRK, TSS_POLICY_USAGE, &hSRKPolicy);
	//Set the SRK policy to be the well known secret
	result=Tspi_Policy_SetSecret(hSRKPolicy,TSS_SECRET_MODE_SHA1,20, wks); 
	// Note: TSS_SECRET_MODE_SHA1 says “Don’t hash this. Just use the 20 bytes as is.
	//-----------------
	
	int i;
	char *filePath = "/home/yg115/test/testForSysdig/trace.scap71";
	long size = getFileSize(filePath);
	BYTE s[size];
	readFile(filePath, size, s);
/*	//check whether the readFile function reads a right binary file, use hexdump as baseline
	for(i=0 ; i<20;++i){
		printf("%02x",s[i]);
	}
*/
	BYTE hash[20];
	HashThis(hContext, &s, size, &hash);

	for(i=0 ; i<19;++i){	//print the hash value
		printf("%02x",*(hash+i));
	}
	printf("\n");

	resetPCR(hContext, 23);
	extendPCR(hContext, 23, hash);

	//-----Postlude
	Tspi_Context_Close(hContext);
	Tspi_Context_FreeMemory(hContext, NULL);
	// this frees memory that was automatically allocated for you
	Tspi_Context_Close(hContext);
	//-----------	
	return 0;
}
