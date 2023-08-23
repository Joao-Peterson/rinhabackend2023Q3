#ifndef _UTILS_HEADER_
#define _UTILS_HEADER_

#include "../facil.io/fiobj.h"
#include "../facil.io/http.h"
#include <stdbool.h>

typedef enum {
	http_status_code_Continue = 100,
	http_status_code_SwitchingProtocols = 101,
	http_status_code_Processing = 102,
	http_status_code_EarlyHints = 103,
	http_status_code_Ok = 200,
	http_status_code_Created = 201,
	http_status_code_Accepted = 202,
	http_status_code_NonAuthoritativeInformation = 203,
	http_status_code_NoContent = 204,
	http_status_code_ResetContent = 205,
	http_status_code_PartialContent = 206,
	http_status_code_MultiStatus = 207,
	http_status_code_AlreadyReported = 208,
	http_status_code_ImUsed = 226,
	http_status_code_MultipleChoices = 300,
	http_status_code_MovedPermanently = 301,
	http_status_code_Found = 302,
	http_status_code_SeeOther = 303,
	http_status_code_NotModified = 304,
	http_status_code_UseProxy = 305,
	http_status_code_Unused = 306,
	http_status_code_TemporaryRedirect = 307,
	http_status_code_PermanentRedirect = 308,
	http_status_code_BadRequest = 400,
	http_status_code_Unauthorized = 401,
	http_status_code_PaymentRequired = 402,
	http_status_code_Forbidden = 403,
	http_status_code_NotFound = 404,
	http_status_code_MethodNotAllowed = 405,
	http_status_code_NotAcceptable = 406,
	http_status_code_ProxyAuthenticationRequired = 407,
	http_status_code_RequestTimeout = 408,
	http_status_code_Conflict = 409,
	http_status_code_Gone = 410,
	http_status_code_LengthRequired = 411,
	http_status_code_PreconditionFailed = 412,
	http_status_code_PayloadTooLarge = 413,
	http_status_code_UriTooLong = 414,
	http_status_code_UnsupportedMediaType = 415,
	http_status_code_RangeNotSatisfiable = 416,
	http_status_code_ExpectationFailed = 417,
	http_status_code_ImATeapot = 418,
	http_status_code_MisdirectedRequest = 421,
	http_status_code_UnprocessableEntity = 422,
	http_status_code_Locked = 423,
	http_status_code_FailedDependency = 424,
	http_status_code_TooEarly = 425,
	http_status_code_UpgradeRequired = 426,
	http_status_code_PreconditionRequired = 428,
	http_status_code_TooManyRequests = 429,
	http_status_code_RequestHeaderFieldsTooLarge = 431,
	http_status_code_UnavailableForLegalReasons = 451,
	http_status_code_InternalServerError = 500,
	http_status_code_NotImplemented = 501,
	http_status_code_BadGateway = 502,
	http_status_code_ServiceUnavailable = 503,
	http_status_code_GatewayTimeout = 504,
	http_status_code_HttpVersionNotSupported = 505,
	http_status_code_VariantAlsoNegotiates = 506,
	http_status_code_InsufficientStorage = 507,
	http_status_code_LoopDetected = 508,
	http_status_code_NotExtended = 510,
	http_status_code_NetworkAuthenticationRequired = 511,
}http_status_codes_t;

bool fiobj_str_cmp(FIOBJ fiobj, char *str);

bool fiobj_str_substr(FIOBJ fiobj, char *str);

#endif