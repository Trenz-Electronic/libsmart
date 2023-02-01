#include <stdlib.h>   // size_t for crack
#include <crack.h>		// password check
#include <crypt.h>		// SHA512 encryption
#include <grp.h>		// for /etc/group
#include <pwd.h>  		// for /etc/password
#include <stdint.h>		// uint8_t
#include <stdio.h>
#include <time.h>		// time functioons
#include <string.h>		// strstr etc.
#include <shadow.h>  	// for /etc/shadow
#include <sys/types.h>  // for struct group
#include <unistd.h>		// sysconf
#include <vector>		// std::vector

#include "File.h" 		// File::renameFile()
#include "memory.h"		// std::make_unique<T>
#include "passwd.h" 	// header for shadow password
#include "string.h" 	// ssprintf

namespace smart {
namespace passwd {


// Size of the buffer for the password record.
static const int PASSWORD_BUFFER_SIZE = sysconf(_SC_GETPW_R_SIZE_MAX);

/// Pick the group Id of groupName out of /etc/group
int getGroupId(const char* groupName)
{
	struct group *grent;
	grent = getgrnam(groupName);
	endgrent();

	return grent ? grent->gr_gid : (-1);
}

/// Find the user in the /etc/shadow file.
/// Returns	Pointer to a UserInfo structure, else nullptr.
std::unique_ptr<UserInfo> getUserByName(const char *user)
{
	const int bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
	std::vector<char> buf(bufsize);
	int s;

	// for password
	struct passwd rpwd;
	struct passwd *presult;

	// search entry for 'user' in /etc/passwd first
	s = getpwnam_r(user, &rpwd, &buf[0], bufsize, &presult);
	endpwent();

	// on error (s != 0)
	if (s)
	{
		throw std::runtime_error(ssprintf("Error %d at getpwnam_r !", s));
	}

	// no entry ?
	if (!presult)
	{
		return nullptr;
	}

	// for shadow
	struct spwd rspwd;
	struct spwd *sresult;

	// lock /etc/shadow
	lckpwdf();

	// search entry for 'user' in /etc/shadow
	s = getspnam_r(user, &rspwd, &buf[0], bufsize, &sresult);
	endspent();
	// unlock
	ulckpwdf();
	// on error (s != 0)
	if (s)
	{
		throw std::runtime_error(ssprintf("Error %d at getspnam_r !", s));
	}

	// no entry ?
	if (!sresult)
	{
		return nullptr;
	}

	// gathering information in structure
	std::unique_ptr<UserInfo> res = std::make_unique<UserInfo>();
	res->username = rspwd.sp_namp;
	res->passwordHash = rspwd.sp_pwdp;
	res->groupId = rpwd.pw_gid;

	return res;
}

void checkIfInUsers(const char *user)
{
	// what GID is 'users' on this system ?
	struct group *grp = getgrnam("users");
	endgrent();
	if (!grp)
	{
		throw std::runtime_error("Group 'users' doesn't exist !");
	}

	// does the user exist ?
	if (!getUserByName(user))
	{
		throw std::runtime_error(ssprintf("User '%s' doesn't exist !", user));
	}

	// get GID of the user from /etc/passwd
	struct passwd *upwd = getpwnam(user);
	if (upwd->pw_gid != grp->gr_gid)
	{
		throw std::runtime_error(ssprintf("User '%s' is not member of group 'users' !", user));
	}
}

std::vector<UserInfo> getUserListByGroup(unsigned int groupId)
{
	struct passwd *pwdstruct = nullptr;
	std::vector<UserInfo> res;

	// open the pwd-stream
	pwdstruct = getpwent();
	while (pwdstruct)
	{
		if (pwdstruct->pw_gid == groupId)
		{
			// we do not make the password hash public here
			UserInfo pwd = { pwdstruct->pw_name, "", pwdstruct->pw_gid };
			res.push_back(pwd);
		}
		pwdstruct = getpwent();
	}
	// close the stream again
	endpwent();

	return res;
}

std::string checkPasswordStrength(const char *pwd)
{
	const unsigned int pwd_length = strlen(pwd);
	if (pwd_length == 0)
	{
		return "empty password";
	}

	std::string msg;
	const char* sep = "";

	// FascistCheck first (dictionary and more)
	const char* v = FascistCheck(pwd, GetDefaultCracklibDict());
	if (v != nullptr)
	{
		msg = v;
		sep = ", ";
	}

	// some more trivial checks
	unsigned int c = 7;
	for (unsigned int idx = 0; idx < pwd_length; ++idx)
	{
		if (c & 4 && (pwd[idx] > 0x2F) && (pwd[idx] < 0x3A))
		{
			c -= 4; // digits
		}
		if (c & 2 && (pwd[idx] > 0x40) && (pwd[idx] < 0x5B))
		{
			c -= 2; // upper case
		}
		if (c & 1 && (pwd[idx] > 0x60) && (pwd[idx] < 0x7B))
		{
			c -= 1; // lower case
		}
	}

	if (c & 1)
	{
		msg += sep;
		msg += "it has no lower case";
		sep = ", ";
	}
	if (c & 2)
	{
		msg += sep;
		msg += "it has no upper case";
		sep = ", ";
	}
	if (c & 4)
	{
		msg += sep;
		msg += "it has no digit";
	}
	return msg;
}


/// Get some (n) new salt from /dev/urandom.
/// \param salt	New salt. Must have space for at least for n+1 characters.
static void generateNewSalt(char *salt, const int n)
{
	// rand() is not that reliable for this task
	FILE* fp = fopen("/dev/urandom", "r");
	File f_auto(fp);
	if (!fp)
	{
		throw std::runtime_error("Opening /dev/urandom failed !");
	}

	int c = 0;
	while ( c < n )
	{
		// get a single character from /dev/urandom
		char r = fgetc(fp);
		if (r < 0x2E || r > 0x7A)
		{
			continue;
		}
		if (r > 0x39 && r < 0x41)
		{
			continue;
		}
		if (r > 0x5A && r < 0x61)
		{
			continue;
		}
		salt[c++] = r;
	}
	salt[c] = '\0';
}


/// Test if an unencrypted password 'pw' is identical with
/// the hashed one given by 'hash' (with prepended salt !).
/// Returns false if fails, true when equal.
bool passwordMatchesHash(const char *pwd, const char *hash)
{
	// get the salt from given hash
	char salt[17] = "";

	// find the terminating '$'
	char *address1 = const_cast<char*>(&hash[4]);
	char *address2 = strstr(address1, "$");
	if (!address2)
	{
		throw std::runtime_error("No valid salt found !");
	}

	// copy salt from hash
	const char *i;
	int j = 0;
	for (i = hash; i < address2; ++i)
	{
		salt[j++] = *i;
	}
	salt[j++] = *i;
	salt[j] = '\0';

	// crypt pwd with salt
	char *result;

	// encryption here
	result = crypt(pwd, salt);
	if (result == nullptr)
	{
		throw std::runtime_error("No result from crypt() !");
	}
	return strcmp(result, hash) == 0;
}

// change the maximum age of the password
void changeMaxPasswordAge(const char *user, int days)
{
	// similar to changePassword
	struct spwd *spwdstruct = nullptr;

	// check if the user exists
	if (!getUserByName(user))
	{
		throw std::runtime_error(ssprintf("User '%s' doesn't exist !", user));
	}
	// open working set of shadow files
	FILE *old_shadow = fopen("/etc/shadow", "r");
	if (!old_shadow)
	{
		throw std::runtime_error("Opening /etc/shadow failed !");
	}
	File of_auto(old_shadow);

	FILE *new_shadow = fopen("/etc/shadow.new", "w");
	if (!new_shadow)
	{
		throw std::runtime_error("Opening /etc/shadow.new failed !");
	}
	File nf_auto(new_shadow);

	// now move entries and change matching
	spwdstruct = fgetspent(old_shadow);
	while (spwdstruct)
	{
		if (!strcmp(spwdstruct->sp_namp, user))
		{
			if ( (spwdstruct->sp_lstchg < 0) && (days > 0) )
			{
				// turn on expiration by setting change time
				time_t sysTime;
				int dateInDays;
				time(&sysTime);
				dateInDays = sysTime / 86400;
				spwdstruct->sp_lstchg = dateInDays;
			}
			spwdstruct->sp_max = days;

			// turn expiration off
			if (days < 0)
			{
				spwdstruct->sp_max = -1;
				spwdstruct->sp_lstchg = -1;
			}
		}
		putspent(spwdstruct, new_shadow);
		spwdstruct = fgetspent(old_shadow);
	}

	// swap and backup them
	File::renameFile("/etc/shadow", "/etc/shadow-");
	File::renameFile("/etc/shadow.new", "/etc/shadow");
}

// check if Password expired
bool passwordExpired(const char *user)
{
	// for time
	time_t sysTime;
	int dateInDays;
	time(&sysTime);
	dateInDays = sysTime / (60 * 60 * 24);

	// for getspnam_r
	int s;
	const int bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
	std::vector<char> buf(bufsize);
	struct spwd rspwd;
	struct spwd *sresult;

	// search entry for 'user' in /etc/shadow
	s = getspnam_r(user, &rspwd, &buf[0], bufsize, &sresult);
	endspent();
	// unlock
	ulckpwdf();

	if (s != 0)
	{
		throw std::runtime_error(ssprintf("passwordExpired: getspname_r error: %d", s));
	}
	// on error : user doesn't exist
	if (sresult == nullptr)
	{
		throw std::runtime_error(ssprintf("User '%s' doesn't exist !", user));
	}

	// Special case : password aging is disabled.
	if (sresult->sp_lstchg == -1)
	{
		return false;
	}

	if (dateInDays > (sresult->sp_lstchg + sresult->sp_max))
	{
		return true;
	}
	else
	{
		return false;
	}
}

/// Set the warning period to the number of days for the user.
void changeWarningPeriod(const char *user, int days)
{
	// similar to changeMaxPasswordAge
	struct spwd *spwdstruct = nullptr;

	// check if the user exists
	if (!getUserByName(user))
	{
		throw std::runtime_error(ssprintf("User '%s' doesn't exist !", user));
	}
	// open working set of shadow files
	FILE *old_shadow = fopen("/etc/shadow", "r");
	if (!old_shadow)
	{
		throw std::runtime_error("Opening /etc/shadow failed !");
	}
	File of_auto(old_shadow);

	FILE *new_shadow = fopen("/etc/shadow.new", "w");
	if (!new_shadow)
	{
		throw std::runtime_error("Opening /etc/shadow.new failed !");
	}
	File nf_auto(new_shadow);

	// now move entries and change matching
	spwdstruct = fgetspent(old_shadow);
	while (spwdstruct)
	{
		if (!strcmp(spwdstruct->sp_namp, user))
		{
			if (days > 0)
				spwdstruct->sp_warn = days;
			else
				spwdstruct->sp_warn = -1;
		}
		putspent(spwdstruct, new_shadow);
		spwdstruct = fgetspent(old_shadow);
	}

	// swap and backup them
	File::renameFile("/etc/shadow", "/etc/shadow-");
	File::renameFile("/etc/shadow.new", "/etc/shadow");
}

// warn before password expires
passwordExpirationInfo getPasswordExpirationInfo(const char *user)
{
	passwordExpirationInfo res;

	// for time
	time_t sysTime;
	int dateInDays;
	time(&sysTime);
	dateInDays = sysTime / (60 * 60 * 24);

	// for getspnam_r
	int s;
	const int bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
	std::vector<char> buf(bufsize);
	struct spwd rspwd;
	struct spwd *sresult;

	// search entry for 'user' in /etc/shadow
	s = getspnam_r(user, &rspwd, &buf[0], bufsize, &sresult);
	endspent();
	// unlock
	ulckpwdf();

	// on error
	if (s != 0)
	{
		throw std::runtime_error(ssprintf("passwordExpired: getspname_r error: %d", s));
	}
	if (sresult == nullptr)
	{
		throw std::runtime_error(ssprintf("User '%s' doesn't exist !", user));
	}

	// this is needed anyway
	res.MaximumAgeDays = sresult->sp_max;
	res.WarningDays = sresult->sp_warn;

	// expiration turned off ?
	if (sresult->sp_max == -1 || sresult->sp_lstchg == -1)
	{
		res.expActive = false;
		res.warnUser = false;
		res.daysLeft = -1;
		return res;
	}
	// else
	res.expActive = true;

	int expDateInDays = sresult->sp_lstchg + sresult->sp_max;

	// no need to warn yet or turned off
	if ( dateInDays < (expDateInDays - sresult->sp_warn) ||  sresult->sp_warn == -1 )
	{
		res.warnUser = false;
		res.daysLeft = -1;
		return res;
	}

	// warn with days left, will be 0 if password expired
	res.warnUser = true;
	res.daysLeft = (expDateInDays - dateInDays) > 0 ? (expDateInDays - dateInDays) : 0;
	return res;
}


/// change a users password
void changePassword(const char *user, const char *pwd)
{
	// similar to delUser
	struct spwd *spwdstruct = nullptr;

	// check if the user exists
	if (!getUserByName(user))
	{
		throw std::runtime_error(ssprintf("User '%s' doesn't exist !", user));
	}

	// new variables for /etc/shadow
	time_t sysTime;
	int dateOfChange;
	char salt[20];
	char *npw;

	// first get salt for SHA512
	strcpy(salt, "$6$");
	generateNewSalt(&salt[3], 8);
	salt[11] = '$';
	salt[12] = '\0';

	npw = crypt(pwd, salt);
	time(&sysTime);
	dateOfChange = sysTime / (60 * 60 * 24);

	// open working set of shadow files
	FILE *old_shadow = fopen("/etc/shadow", "r");
	if (!old_shadow)
	{
		throw std::runtime_error("Opening /etc/shadow failed !");
	}
	File of_auto(old_shadow);

	FILE *new_shadow = fopen("/etc/shadow.new", "w");
	if (!new_shadow)
	{
		throw std::runtime_error("Opening /etc/shadow.new failed !");
	}
	File nf_auto(new_shadow);

	// now move entries and change matching
	spwdstruct = fgetspent(old_shadow);
	while (spwdstruct)
	{
		if (!strcmp(spwdstruct->sp_namp, user))
		{
			strcpy(spwdstruct->sp_pwdp, npw);
			spwdstruct->sp_lstchg = dateOfChange;
		}
		putspent(spwdstruct, new_shadow);
		spwdstruct = fgetspent(old_shadow);
	}

	// swap and backup them
	File::renameFile("/etc/shadow", "/etc/shadow-");
	File::renameFile("/etc/shadow.new", "/etc/shadow");
}

/// Add a new user to the system.
/// Password 'pw' has to be unencrypted.
void addNewUser(const char *user, const char *pwd, int groupId)
{
	struct passwd *pwdstruct = nullptr;

	// first of all: does the user already exist ?
	if (getUserByName(user))
	{
		throw std::runtime_error(ssprintf("User '%s' already exists !", user));
	}
	// find a new UID
	// open the pwd-stream
	pwdstruct = getpwent();
	unsigned int uid = 999;
	while (pwdstruct)
	{
		if (pwdstruct->pw_uid > uid && strcmp(pwdstruct->pw_name, "nobody"))
		{
			uid = pwdstruct->pw_uid;
		}
		pwdstruct = getpwent();
	}
	endpwent();

	// new entry data for /etc/passwd
	struct passwd npwdstruct;
	npwdstruct.pw_name = const_cast<char*>(user);
	// the 'x' means shadow is used
	npwdstruct.pw_passwd = const_cast<char*>("x");
	npwdstruct.pw_uid = ++uid;
	npwdstruct.pw_gid = groupId;
	npwdstruct.pw_gecos = const_cast<char*>("");
	npwdstruct.pw_dir = const_cast<char*>("/");
	npwdstruct.pw_shell = const_cast<char*>("/bin/sh");

	// put the new entries in file now
	FILE *pfd = fopen("/etc/passwd", "a");
	File pf_auto(pfd);
	if (putpwent(&npwdstruct, pfd))
	{
		throw std::runtime_error("Failed to change /etc/passwd !");
	}

	// variables for /etc/shadow
	time_t sysTime;
	int dateOfChange;
	char salt[20];
	char *npw;

	// first get salt for SHA512
	strcpy(salt, "$6$");
	generateNewSalt(&salt[3], 8);
	salt[11] = '$';
	salt[12] = '\0';

	npw = crypt(pwd, salt);
	time(&sysTime);
	dateOfChange=sysTime / (60*60*24);
	// new entry data for /etc/shadow
	struct spwd nspwdstruct;
	nspwdstruct.sp_namp = const_cast<char*>(user);
	nspwdstruct.sp_pwdp = npw;
	nspwdstruct.sp_lstchg = dateOfChange;
	nspwdstruct.sp_min = 0;
	nspwdstruct.sp_max = -1;
	nspwdstruct.sp_warn = -1;
	nspwdstruct.sp_inact = -1;
	nspwdstruct.sp_expire = -1;
	nspwdstruct.sp_flag = -1;

	// put the new entries in file now
	FILE *sfd = fopen("/etc/shadow", "a");
	File sf_auto(sfd);
	if (putspent(&nspwdstruct, sfd))
	{
		throw std::runtime_error("\nFailed to change /etc/shadow !\n");
	}

}

void deleteUser(const char *user)
{
	struct spwd *spwdstruct = nullptr;
	struct passwd *pwdstruct = nullptr;

	// check if the user exists and is in group 'users'
	checkIfInUsers(user);

	// open working set of passwd files
	FILE *old_passwd;
	old_passwd = fopen("/etc/passwd", "r");
	File opf_auto(old_passwd);
	FILE *new_passwd;
	new_passwd = fopen("/etc/passwd.new", "w");
	File npf_auto(new_passwd);

	// now move entries except matching
	pwdstruct = fgetpwent(old_passwd);
	while (pwdstruct)
	{
		if (strcmp(pwdstruct->pw_name, user))
		{
			putpwent(pwdstruct, new_passwd);
		}
		pwdstruct = fgetpwent(old_passwd);
	}

	// swap them
	File::renameFile("/etc/passwd", "/etc/passwd-");
	File::renameFile("/etc/passwd.new", "/etc/passwd");

	// open working set of shadow files
	FILE *old_shadow;
	old_shadow = fopen("/etc/shadow", "r");
	File osf_auto(old_shadow);
	FILE *new_shadow;
	new_shadow = fopen("/etc/shadow.new", "w");
	File nsf_auto(new_shadow);

	// now move entries except matching
	spwdstruct = fgetspent(old_shadow);
	while (spwdstruct)
	{
		if (strcmp(spwdstruct->sp_namp, user))
		{
			putspent(spwdstruct, new_shadow);
		}
		spwdstruct = fgetspent(old_shadow);
	}

	// swap them
	File::renameFile("/etc/shadow", "/etc/shadow-");
	File::renameFile("/etc/shadow.new", "/etc/shadow");
}

} // namespace passwd
} // namespace smart
