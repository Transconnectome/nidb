#!/bin/sh

# create link to the mariadb libraries */
ln -s /lib64/libmariadb.so.3 /lib64/libmysqlclient.so.18

# PHP packages */
pear install Mail Mail_Mime Net_SMTP

# disable SE Linux */
setenforce 0
sed -i s/^SELINUX=.*/SELINUX=disabled/g /etc/selinux/config

# change php.ini settings */
sed -i s/^short_open_tag = .*/short_open_tag = On/g /etc/php.ini
sed -i s/^session.gc_maxlifetime = .*/session.gc_maxlifetime = 28800/g /etc/php.ini
sed -i s/^memory_limit = .*/memory_limit = 5000M/g /etc/php.ini
sed -i s!^;.*upload_tmp_dir = .*!upload_tmp_dir = /nidb/uploadtmp!g /etc/php.ini
sed -i s/^upload_max_filesize = .*/upload_max_filesize = 5000M/g /etc/php.ini
sed -i s/^max_file_uploads = .*/max_file_uploads = 1000/g /etc/php.ini
sed -i s/^;.*max_input_vars = .*/max_input_vars = 1000/g /etc/php.ini # this line is probably commented out */
sed -i s/^max_input_time = .*/max_input_time = 600/g /etc/php.ini
sed -i s/^max_execution_time = .*/max_execution_time = 600/g /etc/php.ini
sed -i s/^post_max_size = .*/post_max_size = 5000M/g /etc/php.ini
sed -i s/^display_errors = .*/display_errors = On/g /etc/php.ini
sed -i s/^error_reporting = .*/error_reporting = E_ALL \\& \\~E_DEPRECATED \\& \\~E_STRICT \\& \\~E_NOTICE/ /etc/php.ini

# enable and start services */
systemctl enable httpd.service   # enable the apache web service */
systemctl enable mariadb.service # enable the MariaDB service */
systemctl enable php-fpm.service # enable PHP-FastCGI Process Manager service */
systemctl start httpd.service
systemctl start mariadb.service
systemctl start php-fpm.service

# make sure port 80 is accessible through the firewall */
firewall-cmd --permanent --add-port=80/tcp
firewall-cmd --reload

# add nidb to the apache group, and apache to the nidb group */
usermod -G apache nidb
usermod -G nidb apache

# change permissions of the /nidb directory */
chown -R nidb:nidb /nidb  # change ownership of the install directory */
chmod -R g+w /nidb        # change permissions of the install directorys contents */
chmod 777 /nidb              # change permissions of the install directory */

# setup cron jobs */
crontab -u nidb /nidb/crontab.txt

# database stuff */
chmod 777 /nidb/mysql.sh
/nidb/mysql.sh

# add dcmrcv service at boot */
cp /nidb/dcmrcv /etc/init.d  # copy the dcmrcv init script */
chmod 755 /etc/init.d/dcmrcv # change permissions of the script */
chkconfig --add dcmrcv       # add the script to start at boot */

# create program directories */
#mkdir -p /nidb/programs/logs
#mkdir -p /nidb/programs/lock

# create data directories */
mkdir -p /nidb/data
mkdir -p /nidb/data/archive
mkdir -p /nidb/data/backup
mkdir -p /nidb/data/deleted
mkdir -p /nidb/data/dicomincoming
mkdir -p /nidb/data/download
mkdir -p /nidb/data/ftp
mkdir -p /nidb/data/problem
mkdir -p /nidb/data/tmp
mkdir -p /nidb/data/upload

Extract", archive, "/var/www/html
chmod -R 755 /var/www/html
chown -R nidb:nidb /var/www/html
