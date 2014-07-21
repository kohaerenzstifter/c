/*static void verifyPost(char *url, int fromParent, int toParent,
		GList *requestHeaders, GList *requestParameters, gboolean *responseQueued,
		err_t *e)
{
	mark();

	char *signedDataFilename = NULL;
	char *signatureFilename = NULL;
	int rc = 0;
	void *content = NULL;
	GList *urlEncodedData = NULL;
	gboolean urlEncoded = FALSE;
	char *signedData = NULL;
	char *signature = NULL;
	char buffer[PATH_MAX];
	char *publicKeyFilename = NULL;
	char publicKey[] =
			"-----BEGIN PUBLIC KEY-----\n"
			"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEArCmkQjGQ691rx2vj6dYB\n"
			"Bh7ReLKkmLa2uaUsOJeeOh7fMgjoRj+TJlYf0t+b4JMxuOVsEMszKACb1OYu63ir\n"
			"YcMbMfAcDzE2zY1UsN4YauQyU869IbgVpG+7KCU18V8hFi9IX3b7CeKJPYkwrxb8\n"
			"Jg0l8sfZOw+6DK9I+64Taqgca8TtGao6KBYKF/Efq9bAlyd9raPy2kODjECTRJYG\n"
			"B5esSPyWI0kMvG+7hrOXHAfJw2ZG9FL71mHT4lQb2lj0rVHGAXI2UroR8wLadGM9\n"
			"oSlSvB1s/K6Syi+uR2SLy2nonqCymr2dSd+ebaQkNbzYU4VcUrcwfCjVy8w3XM0n\n"
			"XQIDAQAB\n"
			"-----END PUBLIC KEY-----";

	terror(urlEncoded = isUrlEncoded(requestHeaders, e))
	terror(failIfFalse(urlEncoded))
	terror(urlEncodedData = readUrlEncodedData(fromParent, e))

	terror(signedData = getValue(urlEncodedData, "signedData", e))
	terror(signature = getValue(urlEncodedData, "signature", e))

	terror(publicKeyFilename = writeToTempFile(publicKey, e))
	terror(signedDataFilename = writeToTempFile(signedData, e))
	terror(signatureFilename = writeToTempFile(signature, e))
	terror(runSync(e, "openssl enc -base64 -d -in %s -A > %s.bin",
			signatureFilename, signatureFilename))
	terror(rc =
			runSync(e, "openssl dgst -sha1 -verify %s -signature %s.bin %s",
			publicKeyFilename, signatureFilename, signedDataFilename))
	terror(failIfFalse(rc == 0))

finish:
	if (publicKeyFilename != NULL) {
		unlink(publicKeyFilename);
		free(publicKeyFilename);
	}
	if (signedDataFilename != NULL) {
		unlink(signedDataFilename);
		free(signedDataFilename);
	}
	if (signatureFilename != NULL) {
		sprintf(buffer, "%s.bin", signatureFilename);
		unlink(buffer);
		unlink(signatureFilename);
		free(signatureFilename);
	}
	freeKeyValuePairs(urlEncodedData);
	free(content);
}*/
