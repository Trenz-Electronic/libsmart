#pragma once
/// \file  passwd.h
/// \brief	Interface to the password and shadow password files.
///
/// \version 	1.0
/// \date		2020
/// \copyright	SPDX: BSD-3-Clause 2016-2017 Trenz Electronic GmbH

#include <string>	// std::string
#include <memory>	// std::unique_ptr
#include <vector>	// std::vector

namespace smart {
namespace passwd {

/// User name and credentials.
struct UserInfo
{
	/// Users login name.
	std::string username;
	/// Encrypted password (SHA512).
	std::string passwordHash;
	/// Group ID.
	unsigned int groupId;
};

/// Informations about password handling.
struct passwordExpirationInfo
{
	/// Does the password expire ?
	bool expActive;
	/// Should the user be warned now ?
	bool warnUser;
	/// How many days are left ?
	int daysLeft;
	/// The maximum password age.
	int MaximumAgeDays;
	/// Days to warn before expiration.
	int WarningDays;
};

/// Pick the group Id of groupName out of /etc/group
/// Note : MT-Unsafe.
/// \param 	groupName	Name of group to look for.
/// \return				Appropriate integer value.
///						Will return -1 when not found.
int getGroupId(const char* groupName);

/// Find the user in the /etc/shadow file.
/// Note : MT-Unsafe.
/// \param user			User to look for in the system.
/// \return				Pointer to UserInfo structure. nullptr if failed.
std::unique_ptr<UserInfo> getUserByName(const char *user);

/// Get a list of valid users by GroupId
/// Note : MT-Unsafe.
/// \param groupId		GroupId for the System.
/// \return 			Vector of type Password.
std::vector<UserInfo> getUserListByGroup(unsigned int groupId);

/// Check whether the password is strong enough.
/// Note : MT-Unsafe.
/// \param pwd			Password to be checked.
/// \return				Information about password strenght.
///						Will be empty string when O.K.
std::string checkPasswordStrength(const char *pwd);

/// Test if an unencrypted password 'pwd' is identical with
/// the hashed one given by 'hash' from /etc/shadow.
/// Note : MT-Unsafe.
/// \param pwd			Unencrypted password.
/// \param hash			Hashed password from /etc/shadow.
/// \return				False if differs, true when equal.
bool passwordMatchesHash(const char *pwd, const char *hash);

/// Change a users password.
/// This will set the date of last change, too.
/// Note : MT-Unsafe.
///	\param user			User whose password is to be changed.
/// \param pwd			The unencrypted new password.
void changePassword(const char *user, const char *pwd);

/// Change the maximum age of the password.
/// Note : MT-Unsafe.
/// \param user			User whose maximum password age will be set.
/// \param days			Maximum age of password in days.
/// 					A value < 0 will turn expiration off.
void changeMaxPasswordAge(const char *user, int days);

/// Chek if the Password expired.
/// Note : MT-Unsafe.
/// \param user			User whose maximum password age will be checked.
/// \return				true if expired, false if valid.
bool passwordExpired(const char *user);

/// Set the warning period to the number of days for the user.
/// Note : MT-Unsafe.
/// \param user			User whose warning period will be set.
/// \param days			Days to warn before password expiration.
void changeWarningPeriod(const char *user, int days);

/// Checks if the user account can expire at all,
/// if the user has to be warned before and
/// number of days left before expiration,
/// will be 0 if expired, -1 if expiration is off
/// Note : MT-Unsafe.
/// \param user			User whose warning period will be checked.
/// \return				passwordExpirationInfo structure.
passwordExpirationInfo getPasswordExpirationInfo(const char *user);

/// Try to add a new user to the system manually.
/// Note : MT-Unsafe.
/// \param user			New user to add to the system.
/// \param pwd			Unencrypted password.
/// \param groupId		GroupId for the UI (0: user, 1: admin).
void addNewUser(const char *user, const char *pwd, int groupId);

/// Remove a user from the system.
/// Note : MT-Unsafe.
/// \param user			User to be removed from shadow and passwd files.
void deleteUser(const char *user);

} // namespace passwd
} // namespace smart
