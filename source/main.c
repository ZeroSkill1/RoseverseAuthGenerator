#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>

#include <3ds.h>

#define MEMBLOCK_SIZE		 0x20000
#define MEMBLOCK_ALIGN		0x1000
#define SERVICETOKEN_MAX_SIZE 512
#define OLIVE_DIR			  "sdmc:/olive"
#define OLIVE_PW_PATH		 OLIVE_DIR "/acc_key.txt"
#define OLIVE_TOKEN_PATH	  OLIVE_DIR "/token.txt"

/**
 * @brief Calculates the maximum Base64 encoded length for a given number of bytes.
 * @details Base64 expands every 3 bytes into 4 characters, plus padding.
 * Add 1 extra for null terminator.
 * @param n Number of input bytes.
 * @return Maximum required output size in bytes.
 */
#define BASE64_ENCODED_SIZE(n) ((((n) + 2) / 3) * 4 + 1)

/**
 * @brief Encodes a binary buffer into Base64 text.
 * @param input Pointer to raw input bytes.
 * @param len Number of bytes in input.
 * @param output Pointer to destination buffer (must be at least BASE64_ENCODED_SIZE(len)).
 */
static void base64_encode(const unsigned char* input, size_t len, char* output) {
	static const char cBase64Alphabet[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";

	size_t outIndex = 0;
	size_t i = 0;

	while (i + 2 < len) {
		// Take 3 bytes and split into 4 groups of 6 bits.
		int triple = (input[i] << 16) | (input[i + 1] << 8) | input[i + 2];
		output[outIndex++] = cBase64Alphabet[(triple >> 18) & 0x3F];
		output[outIndex++] = cBase64Alphabet[(triple >> 12) & 0x3F];
		output[outIndex++] = cBase64Alphabet[(triple >> 6)  & 0x3F];
		output[outIndex++] = cBase64Alphabet[triple & 0x3F];
		i += 3;
	}

	// Handle remaining 1 or 2 bytes with padding.
	if (i < len) {
		int triple = input[i] << 16;
		if (i + 1 < len) {
			triple |= input[i + 1] << 8;
		}

		output[outIndex++] = cBase64Alphabet[(triple >> 18) & 0x3F];
		output[outIndex++] = cBase64Alphabet[(triple >> 12) & 0x3F];

		if (i + 1 < len) {
			output[outIndex++] = cBase64Alphabet[(triple >> 6) & 0x3F];
			output[outIndex++] = '=';
		} else {
			output[outIndex++] = '=';
			output[outIndex++] = '=';
		}
	}

	// Null terminate.
	output[outIndex] = '\0';
}

// Generate random alphanumeric password
static void gen_olive_user_key(char *out, size_t out_len, size_t length)
{
	static const char charset[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";
	static const size_t charset_len = sizeof(charset) - 1;

	if (length >= out_len)
		length = out_len - 1;

	u64 seed = (u64)svcGetSystemTick();
	for (size_t i = 0; i < length; i++)
	{
		seed = seed * 1664525 + 1013904223; // LCG step
		out[i] = charset[seed % charset_len];
	}
	out[length] = '\0';
}

static void obfuscate_string(char* str, size_t len)
{
	static const char secret[] = "" KEY_SECRET "";
	#define SECRET_LEN (sizeof(secret) - 1) // exclude null terminator

	for (size_t i = 0; i < len; ++i)
	{
		str[i] ^= secret[i % SECRET_LEN]; // XOR each byte with secret
	}
}

Result genOrLoadToken(bool *out_ok, bool *had_password) {
	Result res = 0;
	*out_ok = false;
	
	u8 account_slot = 0;
	if (R_FAILED(res = ACT_GetCommonInfo(&account_slot, sizeof(account_slot), INFO_TYPE_COMMON_CURRENT_ACCOUNT_SLOT))) {
		fprintf(stderr, "Could not get current account slot.\n");
		return res;
	}
	
	if (account_slot == 0) {
		fprintf(stderr, "No PNID is loaded.\n");
		return res;
	}
	
	char mii_image_url[0x101] = { 0 };
	if (R_FAILED(res = ACT_GetAccountInfo(&mii_image_url, sizeof(mii_image_url), ACT_DEFAULT_ACCOUNT, INFO_TYPE_MII_IMAGE_URL))) {
		fprintf(stderr, "Could not get account Mii image URL.\n");
		return res;
	}
	
	if (!strstr(mii_image_url, "pretendo.cc")) {
		fprintf(stderr, "Current NNID doesn't seem to be a PNID.\nPlease make sure you have switched to Pretendo.\n");
		return res;
	}
	
	u32 pid = 0;
	char password[256] = {0};
	char country[3] = { 0 };
	u8 gender = 0;
	BirthDate birthdate = { 0 };
	char serial[16] = { 0 };
	
	if (R_FAILED(res = ACT_GetAccountInfo(&pid, sizeof(pid), ACT_DEFAULT_ACCOUNT, INFO_TYPE_PRINCIPAL_ID))) {
		fprintf(stderr, "Failed getting PNID PrincipalId.\n");
		return res;
	}

	{
		FILE *in = fopen(OLIVE_PW_PATH, "r");
		if (in != NULL)
		{
			fgets(password, sizeof(password), in);
			fclose(in);
			// Remove newline if present
			size_t len = strlen(password);
			if (len > 0 && password[len - 1] == '\n')
				password[len - 1] = '\0';
			
			*had_password = true;
			//printf("DEBUG: read existing password:\n%s\n", password);
		}
		else
		{
			gen_olive_user_key(password, sizeof(password), 20);
			FILE *out = fopen(OLIVE_PW_PATH, "w");
			if (out != NULL)
			{
				fputs(password, out);
				fclose(out);
			}
			//printf("DEBUG: generated new password:\n%s\n", password);
		}
	}
	
	if (R_FAILED(res = ACT_GetAccountInfo(&country, sizeof(country), ACT_DEFAULT_ACCOUNT, INFO_TYPE_COUNTRY_NAME))) {
		fprintf(stderr, "Could not PNID country.\n");
		return res;
	}
	
	if (R_FAILED(res = ACT_GetAccountInfo(&gender, sizeof(gender), ACT_DEFAULT_ACCOUNT, INFO_TYPE_GENDER))) {
		fprintf(stderr, "Could not PNID gender.\n");
		return res;
	}
	
	if (R_FAILED(res = ACT_GetAccountInfo(&birthdate, sizeof(birthdate), ACT_DEFAULT_ACCOUNT, INFO_TYPE_BIRTH_DATE))) {
		fprintf(stderr, "Could not PNID birthday.\n");
		return res;
	}
	
	if (R_FAILED(res = CFGI_SecureInfoGetSerialNumber((u8 *)serial))) {
		fprintf(stderr, "Could not retrieve console serial.\n");
		return res;
	}
	
	char token[SERVICETOKEN_MAX_SIZE+1] = { 0 };
	char b64token[SERVICETOKEN_MAX_SIZE+1] = { 0 };
	size_t offset = 0;
	offset += snprintf(
		token + offset, sizeof(token) - offset, "%lu,%s,%s,%u,%u,%u/%u/%u,%s,0",
		pid, password, country, gender, 3,
		birthdate.year, birthdate.month, birthdate.day,
		serial);
	
	//printf("debug: token=\n%s\n", token);
	
	obfuscate_string(token, offset);
	base64_encode((const uint8_t*)token, offset, b64token);
	
	//printf("debug: tokenb64=\n%s\n", b64token);
	
	FILE *output = fopen(OLIVE_TOKEN_PATH, "wb");
	*out_ok = fputs(token, output) >= 0;
	fclose(output);
	if (!*out_ok) {
		fprintf(stderr, "Could not write token file to SD card.\n");
	}
	return res;
}

int main(int argc, char* argv[])
{
	void *act_heapmem = NULL;
	Handle act_heapmem_block = 0;
	Result res = 0;
	
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);
	
	mkdir(OLIVE_DIR, 0777);
	
	if (R_FAILED(res = actInit(true))) {
		printf("Could not initialize act: %08lX\n", res);
		goto exit;
	}
	
	if (R_FAILED(res = cfguInit())) {
		printf("Could not initialize cfg: %08lX\n", res);
		goto exit;
	}
	
	act_heapmem = memalign(MEMBLOCK_ALIGN, MEMBLOCK_SIZE);
	if (!act_heapmem) {
		fprintf(stderr, "Couldn't allocate heap memory for act!\n");
		goto exit;
	}

	if (R_FAILED(res = svcCreateMemoryBlock(&act_heapmem_block, (u32)act_heapmem, MEMBLOCK_SIZE, 0, MEMPERM_READWRITE))) {
		printf("Could not create memblock: %08lX\n", res);
		goto exit;
	}

	if (R_FAILED(res = ACT_Initialize(0x90C00C8, MEMBLOCK_SIZE, act_heapmem_block))) {
		printf("Could not initialize act:u: %08lX\n", res);
		goto exit;
	}
	
	bool ok = false, had_password = false;
	res = genOrLoadToken(&ok, &had_password);
	
	if (R_FAILED(res)) {
		printf("Result code: %08lX\n", res);
	} else if (R_SUCCEEDED(res) && ok) {
		if (had_password)
			printf("The token was updated successfully.\n");
		else
			printf("The token was created successfully.\n");
	} else {
		printf("Unable to generate token. Please try again.\n");
	}

exit:
	actExit();
	cfguExit();
	if (act_heapmem_block) svcCloseHandle(act_heapmem_block);
	if (act_heapmem) free(act_heapmem);
	
	printf("Press START to exit.\n");
	
	while (aptMainLoop())
	{
		gspWaitForVBlank();
		gfxSwapBuffers();
		hidScanInput();

		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
			break;
	}

	gfxExit();
	return 0;
}
