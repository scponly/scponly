#!/bin/bash
#made by Konrad Krzysztof Krasinski 2003
#3rd october 2003 - updated for RH9 support

echo -n "Install for what username? "
read user

if [ "x$user" = "x" ]
then
    echo "bad username !!!"
    exit
fi

echo
userhome=/home/$user
echo -n "enter the home directory you wish to set for this user: [$userhome] "
read userhome2
if [ "x$userhome2" != "x" ]; then
    userhome=$userhome2
fi

szablon=template_scp
szablonhome=/home/$szablon

#userdel $user
#rm -rf $userhome

mkdir -p $userhome
useradd -d $userhome//public_ftp -s /usr/local/sbin/scponlyc $user

for x in `find $szablonhome -type d`
do
    y=`echo $x | awk '{print substr($1,length("'$szablonhome'")+2)}'`
    echo mkdir:   $y
    mkdir -p $userhome/$y
done

for x in `find $szablonhome -type f | grep -v public_ftp | grep -v /etc/passwd | grep -v /etc/group`
do
#    echo $x
    y=`echo $x | awk '{print substr($1,length("'$szablonhome'")+2)}'`
    echo ln:   $y
    ln $szablonhome/$y $userhome/$y
done

echo /etc/passwd - important security fix
useruid=`id -u $user`
usergid=`id -g $user`
cat /etc/passwd | awk -F":" '{if($3==0){print $0}}' > $userhome/etc/passwd
#winscp seems to work bad with long names with "_" char - like "template_scp"
#so we cheats it by standard "user" name
dummyuser="user"
dummyhome="/public_ftp"
dummyshell="/usr/bin/oafish"
cat /etc/passwd | awk -F":" '{if($3=='$useruid'){print "'$dummyuser':"$2":"$3":"$4":"$5":'$dummyhome':'$dummyshell'"}}' >> $userhome/etc/passwd

echo /etc/group - adding
cat /etc/group | awk -F":" '{if($3==0){print $0}}' > $userhome/etc/group
dummygroup="users"
cat /etc/group | awk -F":" '{if($3=='$usergid'){print "'$dummygroup':"$2":"$3":"$4}}' >> $userhome/etc/group

echo clearing new home dir ...
rm -rf $userhome/public_ftp
mkdir -p $userhome/public_ftp
chown -R $user.users $userhome/public_ftp

#edquota -p $szablon $user

passwd $user
