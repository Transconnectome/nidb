/* ------------------------------------------------------------------------------
  NIDB moduleExport.cpp
  Copyright (C) 2004 - 2020
  Gregory A Book <gregory.book@hhchealth.org> <gregory.a.book@gmail.com>
  Olin Neuropsychiatry Research Center, Hartford Hospital
  ------------------------------------------------------------------------------
  GPLv3 License:

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
  ------------------------------------------------------------------------------ */

#include "moduleExport.h"
#include <QSqlQuery>


/* ---------------------------------------------------------- */
/* --------- moduleExport ----------------------------------- */
/* ---------------------------------------------------------- */
moduleExport::moduleExport(nidb *a)
{
	n = a;
}


/* ---------------------------------------------------------- */
/* --------- ~moduleFileIO ---------------------------------- */
/* ---------------------------------------------------------- */
moduleExport::~moduleExport()
{

}


/* ---------------------------------------------------------- */
/* --------- Run -------------------------------------------- */
/* ---------------------------------------------------------- */
int moduleExport::Run() {
	n->WriteLog("Entering the export module");

	/* get list of things to delete */
	QSqlQuery q("select * from exports where status = 'submitted'");
	n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);

	if (q.size() > 0) {
		int i = 0;
		while (q.next()) {
			n->ModuleRunningCheckIn();
			if (!n->ModuleCheckIfActive()) { n->WriteLog("Module is now inactive, stopping the module"); return 0; }
			bool found = false;
			QString msg;
			i++;

			int exportid = q.value("export_id").toInt();
			QString username = q.value("username").toString().trimmed();
			QString exporttype = q.value("destinationtype").toString().trimmed();
			bool downloadimaging = q.value("download_imaging").toBool();
			bool downloadbeh = q.value("download_beh").toBool();
			bool downloadqc = q.value("download_qc").toBool();
			QString nfsdir = q.value("nfsdir").toString().trimmed();
			QString filetype = q.value("filetype").toString().trimmed();
			QString dirformat = q.value("dirformat").toString().trimmed();
			int preserveseries = q.value("do_preserveseries").toInt();
			bool gzip = q.value("do_gzip").toBool();
			int anonymize = q.value("anonymization_level").toInt();
			QString behformat = q.value("beh_format").toString().trimmed();
			QString behdirrootname = q.value("beh_dirrootname").toString().trimmed();
			QString behdirseriesname = q.value("beh_dirseriesname").toString().trimmed();
			QString remoteftpusername = q.value("remoteftp_username").toString().trimmed();
			QString remoteftppassword = q.value("remoteftp_password").toString().trimmed();
			QString remoteftpserver = q.value("remoteftp_server").toString().trimmed();
			QString remoteftpport = q.value("remoteftp_port").toString().trimmed();
			QString remoteftppath = q.value("remoteftp_path").toString().trimmed();
			int remotenidbconnid = q.value("remotenidb_connectionid").toInt();
			int publicdownloadid = q.value("publicdownloadid").toInt();
			QString bidsreadme = q.value("bidsreadme").toString().trimmed();

			/* remove a trailing slash if it exists */
			if (nfsdir.right(1) == "/")
				nfsdir.chop(1);

			/* get the current status of this fileio request, make sure no one else is processing it, and mark it as being processed if not */
			QString status = GetExportStatus(exportid);
			if (status == "submitted") {
				/* set the status. if something is wrong, skip this request */
				if (!SetExportStatus(exportid, "processing")) {
					n->WriteLog(QString("Unable to set export status to [%1]").arg(status));
					continue;
				}
			}
			else {
				/* skip this IO request... the status was changed outside of this instance of the program */
				n->WriteLog(QString("The status for this export [%1] has been changed from [submitted] to [%2]. Skipping.").arg(exportid).arg(status));
				continue;
			}

			n->WriteLog("");
			n->WriteLog(QString(" ---------- Export operation (%1 of %2) ---------- ").arg(i).arg(q.size()));
			n->WriteLog("");

			QString log;

			if (exporttype == "web") {
				found = ExportLocal(exportid, exporttype, "", 0, downloadimaging, downloadbeh, downloadqc, filetype, dirformat, preserveseries, gzip, anonymize, behformat, behdirrootname, behdirseriesname, status, log);
			}
			else if (exporttype == "publicdownload") {
				found = ExportLocal(exportid, exporttype, "", publicdownloadid, downloadimaging, downloadbeh, downloadqc, filetype, dirformat, preserveseries, gzip, anonymize, behformat, behdirrootname, behdirseriesname, status, log);
			}
			else if (exporttype == "nfs") {
				found = ExportLocal(exportid, exporttype, nfsdir, 0, downloadimaging, downloadbeh, downloadqc, filetype, dirformat, preserveseries, gzip, anonymize, behformat, behdirrootname, behdirseriesname, status, log);
			}
			else if (exporttype == "localftp") {
				found = ExportLocal(exportid, exporttype, nfsdir, 0, downloadimaging, downloadbeh, downloadqc, filetype, dirformat, preserveseries, gzip, anonymize, behformat, behdirrootname, behdirseriesname, status, log);
			}
			else if (exporttype == "export") {
				//found = ExportNiDB(exportid);
			}
			else if (exporttype == "ndar") {
				found = ExportNDAR(exportid, 0, status, log);
			}
			else if (exporttype == "ndarcsv") {
				found = ExportNDAR(exportid, 1, status, log);
			}
			else if (exporttype == "bids") {
				found = ExportBIDS(exportid, bidsreadme, status, log);
			}
			else if (exporttype == "remotenidb") {
				remoteNiDBConnection conn(remotenidbconnid, n);
				if (conn.isValid)
					found = ExportToRemoteNiDB(exportid, conn, status, log);
				else
					n->WriteLog("Invalid remote connection [" + conn.msg + "]");
			}
			else if (exporttype == "remoteftp") {
				found = ExportToRemoteFTP(exportid, remoteftpusername, remoteftppassword, remoteftpserver, remoteftpport.toInt(), remoteftppath, status, log);
			}
			else {
				log += n->WriteLog(QString("Unknown export type [%1]").arg(exporttype));
				status = "error";
			}

			if (!SetExportStatus(exportid,status,log))
				n->WriteLog(QString("Unable to set export status to [%1]").arg(status));

            n->WriteLog(QString("Found []").arg(found));
        }
		n->WriteLog("Finished performing exports");
	}
	else {
		n->WriteLog("Nothing to do");
		return 0;
	}

    return 1;
}


/* ---------------------------------------------------------- */
/* --------- GetExportStatus -------------------------------- */
/* ---------------------------------------------------------- */
QString moduleExport::GetExportStatus(int exportid) {
	QSqlQuery q;
	q.prepare("select status from exports where export_id = :id");
	q.bindValue(":id", exportid);
	n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
	q.first();
	QString status = q.value("status").toString();
	return status;
}


/* ---------------------------------------------------------- */
/* --------- SetExportStatus -------------------------------- */
/* ---------------------------------------------------------- */
bool moduleExport::SetExportStatus(int exportid, QString status, QString msg) {

	if (((status == "pending") || (status == "deleting") || (status == "complete") || (status == "error") || (status == "processing") || (status == "cancelled") || (status == "canceled")) && (exportid > 0)) {
		if (msg.trimmed() == "") {
			QSqlQuery q;
			q.prepare("update exports set status = :status where export_id = :id");
			q.bindValue(":id", exportid);
			q.bindValue(":status", status);
			n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
		}
		else {
			QSqlQuery q;
			q.prepare("update exports set status = :status, log = :msg where export_id = :id");
			q.bindValue(":id", exportid);
			q.bindValue(":msg", msg);
			q.bindValue(":status", status);
			n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
		}
		return true;
	}
	else {
		return false;
	}
}


/* ---------------------------------------------------------- */
/* --------- SetExportSeriesStatus -------------------------- */
/* ---------------------------------------------------------- */
bool moduleExport::SetExportSeriesStatus(int exportseriesid, QString status, QString msg) {

	if (((status == "pending") || (status == "deleting") || (status == "complete") || (status == "error") || (status == "processing") || (status == "cancelled") || (status == "canceled")) && (exportseriesid > 0)) {
		if (msg.trimmed() == "") {
			QSqlQuery q;
			q.prepare("update exportseries set status = :status where exportseries_id = :id");
			q.bindValue(":id", exportseriesid);
			q.bindValue(":status", status);
			n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
		}
		else {
			QSqlQuery q;
			q.prepare("update exportseries set status = :status, statusmessage = :msg where exportseries_id = :id");
			q.bindValue(":id", exportseriesid);
			q.bindValue(":msg", msg);
			q.bindValue(":status", status);
			n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
		}
		return true;
	}
	else {
		return false;
	}
}


/* ---------------------------------------------------------- */
/* --------- GetExportSeriesList ---------------------------- */
/* ---------------------------------------------------------- */
bool moduleExport::GetExportSeriesList(int exportid) {

	QSqlQuery q;
	q.prepare("select * from exportseries where export_id = :exportid");
	q.bindValue(":exportid",exportid);
	n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
	if (q.size() > 0) {
        n->WriteLog(QString("Found [%1] rows for exportID [%2]").arg(q.size()).arg(exportid));
		while (q.next()) {
			QString modality = q.value("modality").toString().toLower();
			int seriesid = q.value("series_id").toInt();
			int exportseriesid = q.value("exportseries_id").toInt();
			QString status = q.value("status").toString();

			QSqlQuery q2;
			q2.prepare(QString("select a.*, b.*, c.enrollment_id, d.project_name, e.uid, e.subject_id from %1_series a left join studies b on a.study_id = b.study_id left join enrollment c on b.enrollment_id = c.enrollment_id left join projects d on c.project_id = d.project_id left join subjects e on e.subject_id = c.subject_id where a.%1series_id = :seriesid order by uid, study_num, series_num").arg(modality));
			q2.bindValue(":seriesid",seriesid);
			n->SQLQuery(q2, __FUNCTION__, __FILE__, __LINE__);

			if (q2.size() > 0) {
				while (q2.next()) {
					QString uid = q2.value("uid").toString();
					int subjectid = q2.value("subject_id").toInt();
					int studynum = q2.value("study_num").toInt();
					int studyid = q2.value("study_id").toInt();
					QString studydatetime = q2.value("study_datetime").toDateTime().toString("yyyyMMdd_HHmmss");
					int seriesnum = q2.value("series_num").toInt();
					int seriessize = q2.value("series_size").toInt();
					QString seriesnotes = q2.value("series_notes").toString();
					QString seriesdesc = q2.value("series_desc").toString();
					QString seriesaltdesc = q2.value("series_altdesc").toString();
					QString projectname = q2.value("project_name").toString();
					QString studyaltid = q2.value("study_alternateid").toString();
					QString studytype = q2.value("study_type").toString();
					QString datatype = q2.value("data_type").toString();
                    if (datatype == "") /* If the modality is MR, the datatype will have a value (dicom, nifti, parrec), otherwise we will set the datatype to the modality */
						datatype = modality;
					int numfiles = q2.value("numfiles").toInt();
					if (modality != "mr")
						numfiles = q2.value("series_numfiles").toInt();
					int numfilesbeh = q2.value("numfiles_beh").toInt();
					int enrollmentid = q2.value("enrollment_id").toInt();

					QString datadir = QString("%1/%2/%3/%4/%5").arg(n->cfg["archivedir"]).arg(uid).arg(studynum).arg(seriesnum).arg(datatype);
					QString behdir = QString("%1/%2/%3/%4/beh").arg(n->cfg["archivedir"]).arg(uid).arg(studynum).arg(seriesnum);
					QString qcdir = QString("%1/%2/%3/%4/qa").arg(n->cfg["archivedir"]).arg(uid).arg(studynum).arg(seriesnum);

					s[uid][studynum][seriesnum]["exportseriesid"] = QString("%1").arg(exportseriesid);
					s[uid][studynum][seriesnum]["seriesid"] = QString("%1").arg(seriesid);
					s[uid][studynum][seriesnum]["subjectid"] = QString("%1").arg(subjectid);
					s[uid][studynum][seriesnum]["studyid"] = QString("%1").arg(studyid);
					s[uid][studynum][seriesnum]["studydatetime"] = studydatetime;
					s[uid][studynum][seriesnum]["modality"] = modality;
					s[uid][studynum][seriesnum]["seriessize"] = QString("%1").arg(seriessize);
					s[uid][studynum][seriesnum]["seriesnotes"] = seriesnotes;
					s[uid][studynum][seriesnum]["seriesdesc"] = seriesdesc;
					s[uid][studynum][seriesnum]["seriesaltdesc"] = seriesaltdesc;
					s[uid][studynum][seriesnum]["numfilesbeh"] = QString("%1").arg(numfilesbeh);
					s[uid][studynum][seriesnum]["numfiles"] = QString("%1").arg(numfiles);
					s[uid][studynum][seriesnum]["projectname"] = projectname;
					s[uid][studynum][seriesnum]["studyaltid"] = studyaltid;
					s[uid][studynum][seriesnum]["studytype"] = studytype;
					s[uid][studynum][seriesnum]["datatype"] = datatype;
					s[uid][studynum][seriesnum]["datadir"] = datadir;
					s[uid][studynum][seriesnum]["behdir"] = behdir;
					s[uid][studynum][seriesnum]["qcdir"] = qcdir;

					/* Check if source data directories exist */
					if (QDir(datadir).exists()) {
						s[uid][studynum][seriesnum]["datadirexists"] = "1";

						if (QDir(datadir).entryInfoList(QDir::NoDotAndDotDot|QDir::AllEntries).count() == 0)
							s[uid][studynum][seriesnum]["datadirempty"] = "1";
						else
							s[uid][studynum][seriesnum]["datadirempty"] = "0";
					}
					else
						s[uid][studynum][seriesnum]["datadirexists"] = "0";

					if (QDir(behdir).exists()) {
						s[uid][studynum][seriesnum]["behdirexists"] = "1";

						if (QDir(behdir).entryInfoList(QDir::NoDotAndDotDot|QDir::AllEntries).count() == 0)
							s[uid][studynum][seriesnum]["behdirempty"] = "1";
						else
							s[uid][studynum][seriesnum]["behdirempty"] = "0";
					}
					else
						s[uid][studynum][seriesnum]["behdirexists"] = "0";

					if (QDir(qcdir).exists()) {
						s[uid][studynum][seriesnum]["qcdirexists"] = "1";

						if (QDir(qcdir).entryInfoList(QDir::NoDotAndDotDot|QDir::AllEntries).count() == 0)
							s[uid][studynum][seriesnum]["qcdirempty"] = "1";
						else
							s[uid][studynum][seriesnum]["qcdirempty"] = "0";
					}
					else
						s[uid][studynum][seriesnum]["qcdirexists"] = "0";

					/* get any alternate IDs */
					QStringList altuids;
					QString primaryaltuid;

					QSqlQuery q3;
					q3.prepare("select altuid, isprimary from subject_altuid where enrollment_id = :enrollmentid and subject_id = :subjectid");
					q3.bindValue(":enrollmentid",enrollmentid);
					q3.bindValue(":subjectid",subjectid);
					n->SQLQuery(q3, __FUNCTION__, __FILE__, __LINE__);
					if (q3.size() > 0) {
						while (q3.next()) {
							altuids << q3.value("altuid").toString();
							if (q3.value("isprimary").toBool())
								primaryaltuid = q3.value("altuid").toString();
						}
						s[uid][studynum][seriesnum]["primaryaltuid"] = primaryaltuid;
						s[uid][studynum][seriesnum]["altuids"] = altuids.join(",");
					}
				}
			}
			else {
				n->WriteLog(QString("No rows found for this seriesid [%1] and modality [%2]").arg(seriesid).arg(modality));
			}
		}
	}
	else {
		n->WriteLog(QString("No series rows found for this exportid [%1]").arg(exportid));
	}

	return true;
}


/* ---------------------------------------------------------- */
/* --------- ExportLocal ------------------------------------ */
/* ---------------------------------------------------------- */
bool moduleExport::ExportLocal(int exportid, QString exporttype, QString nfsdir, int publicdownloadid, bool downloadimaging, bool downloadbeh, bool downloadqc, QString filetype, QString dirformat, int preserveseries, bool gzip, int anonlevel, QString behformat, QString behdirrootname, QString behdirseriesname, QString &exportstatus, QString &msg) {

	QStringList msgs;
	if (!GetExportSeriesList(exportid)) {
		msg = "Unable to get a series list";
		return false;
	}

	QString tmpexportdir = n->cfg["tmpdir"] + "/" + n->GenerateRandomString(20);

	exportstatus = "complete";
	int laststudynum = 0;
	QString newseriesnum = "1";

	/* iterate through the UIDs */
	for(QMap<QString, QMap<int, QMap<int, QMap<QString, QString>>>>::iterator a = s.begin(); a != s.end(); ++a) {
		QString uid = a.key();

		/* iterate through the studynums */
		for(QMap<int, QMap<int, QMap<QString, QString>>>::iterator b = s[uid].begin(); b != s[uid].end(); ++b) {
			int studynum = b.key();

			/* iterate through the seriesnums */
			for(QMap<int, QMap<QString, QString>>::iterator c = s[uid][studynum].begin(); c != s[uid][studynum].end(); ++c) {
				int seriesnum = c.key();

				int exportseriesid = s[uid][studynum][seriesnum]["exportseriesid"].toInt();
				SetExportSeriesStatus(exportseriesid, "processing");

				QString seriesstatus = "complete";
				QString statusmessage;

				//int subjectid = s[uid][studynum][seriesnum]["subjectid"].toInt();
				int seriesid = s[uid][studynum][seriesnum]["seriesid"].toInt();
				QString primaryaltuid = s[uid][studynum][seriesnum]["primaryaltuid"];
				QString altuids = s[uid][studynum][seriesnum]["altuids"];
				QString projectname = s[uid][studynum][seriesnum]["projectname"];
				//int studyid = s[uid][studynum][seriesnum]["studyid"].toInt();
				QString studytype = s[uid][studynum][seriesnum]["studytype"];
				QString studyaltid = s[uid][studynum][seriesnum]["studyaltid"];
				QString modality = s[uid][studynum][seriesnum]["modality"];
				//int seriessize = s[uid][studynum][seriesnum]["seriessize"].toInt();
				QString seriesdesc = s[uid][studynum][seriesnum]["seriesdesc"];
				QString datatype = s[uid][studynum][seriesnum]["datatype"];
				QString indir = s[uid][studynum][seriesnum]["datadir"];
				QString behindir = s[uid][studynum][seriesnum]["behdir"];
				QString qcindir = s[uid][studynum][seriesnum]["qcdir"];
				int numfiles = s[uid][studynum][seriesnum]["numfiles"].toInt();
				bool datadirexists = s[uid][studynum][seriesnum]["datadirexists"].toInt();
				bool behdirexists = s[uid][studynum][seriesnum]["behdirexists"].toInt();
				bool qcdirexists = s[uid][studynum][seriesnum]["qcdirexists"].toInt();
				bool datadirempty = s[uid][studynum][seriesnum]["datadirempty"].toInt();
				//bool behdirempty = s[uid][studynum][seriesnum]["behdirempty"].toInt();
				//bool qcdirempty = s[uid][studynum][seriesnum]["qcdirempty"].toInt();

				/* format the subject/study part of the output directory path */
				QString subjectdir;
				if (dirformat == "shortid")
					subjectdir = QString("%1%2").arg(uid).arg(studynum);
				else if (dirformat == "shortstudyid")
					subjectdir = QString("%1/%2").arg(uid).arg(studynum);
				else if (dirformat == "altuid")
					if (primaryaltuid == "")
						subjectdir = uid;
					else
						subjectdir = primaryaltuid;
				else
					subjectdir = QString("%1%2").arg(uid).arg(studynum);

				/* format the series number part of the output path */
				switch (preserveseries) {
				case 0:
					    if (laststudynum != studynum)
							newseriesnum = "1";
						else
							newseriesnum = QString("%1").arg(newseriesnum.toInt() + 1);
					break;
				case 1:
					    newseriesnum = QString("%1").arg(seriesnum);
					break;
				case 2:
					QString seriesdir = seriesdesc;
					seriesdir.replace(QRegularExpression("[^a-zA-Z0-9_-]"),"_");
					newseriesnum = QString("%1_%2").arg(seriesnum).arg(seriesdir);
				}

				/* format the base directory structure of the output path */
				n->WriteLog(QString("Series number [%1] --> [%2]").arg(seriesnum).arg(newseriesnum));
				msgs << QString("%1 - Series number [%2] --> [%3]").arg(subjectdir).arg(seriesnum).arg(newseriesnum);
				QString rootoutdir;
				if (exporttype == "nfs")
					rootoutdir = QString("%1%2/%3").arg(n->cfg["mountdir"]).arg(nfsdir).arg(subjectdir);
				else if ((exporttype == "web") || (exporttype == "publicdownload"))
					rootoutdir = QString("%1/%2").arg(tmpexportdir).arg(subjectdir);
				else if (exporttype == "localftp")
					rootoutdir = QString("%1/NiDB-%2/%3").arg(n->cfg["ftpdir"]).arg(exportid).arg(subjectdir);
				else
					rootoutdir = QString("%1/%2").arg(tmpexportdir).arg(subjectdir);

				/* make the output directory */
				QDir d;
                if (d.mkpath(rootoutdir)) {
					n->WriteLog(QString("Created rootoutdir [%1]").arg(rootoutdir));
					msgs << "Created rootoutdir [" + rootoutdir + "]. Writing data to directory";
                    QStringList dirparts = rootoutdir.split("/", Qt::SkipEmptyParts);
                    QString dirpath = "";
                    foreach (QString part, dirparts) {
                        dirpath = dirpath + "/" + part;
                        QString systemstring = "chmod -f 777 " + dirpath;
                        n->WriteLog(n->SystemCommand(systemstring, true));
                    }
				}
				else {
					seriesstatus = "error";
					exportstatus = "error";
					n->WriteLog("ERROR unable to create rootoutdir [" + rootoutdir + "]");
					msgs << "Unable to create output directory [" + rootoutdir + "]";
					statusmessage = "Unable to create rootoutdir [" + rootoutdir + "]";
				}

				/* create the behavioral dir output path */
				QString outdir = QString("%1/%2").arg(rootoutdir).arg(newseriesnum);
				QString qcoutdir = QString("%1/qa").arg(outdir);
				QString behoutdir;
				if (behformat == "behroot")
					behoutdir = rootoutdir;
				else if (behformat == "behrootdir")
					behoutdir = rootoutdir + "/" + behdirrootname;
				else if (behformat == "behseries")
					behoutdir = outdir;
				else if (behformat == "behseriesdir")
					behoutdir = outdir + "/" + behdirseriesname;
				else
					behoutdir = rootoutdir;

                n->WriteLog(QString("Export type is '%1'. rootoutdir [%2], outdir [%3], qcoutdir [%4], behoutdir [%5]").arg(exporttype).arg(rootoutdir).arg(outdir).arg(qcoutdir).arg(behoutdir));

				/* export the imaging data */
				if (downloadimaging) {
                    n->WriteLog("Downloading imaging data");
					if (numfiles > 0) {
                        n->WriteLog(QString("Series contains [%1] files").arg(numfiles));
						if (datadirexists) {
                            n->WriteLog("Series data directory [" + indir + "] exists");
                            if (!datadirempty) {
                                n->WriteLog("Data directory is empty");
								// output the correct file type
								if ((modality != "mr") || (filetype == "dicom") || ((datatype != "dicom") && (datatype != "parrec"))) {
									// use rsync instead of cp because of the number of files limit
									QString systemstring = QString("rsync %1/* %2/").arg(indir).arg(outdir);
									n->WriteLog(n->SystemCommand(systemstring));
									msgs << "Copying raw data from [" + indir + "] to [" + outdir + "]";
								}
								else if (filetype == "qc") {
									/* copy only the qc data */
									QString systemstring = QString("cp -R %1/qa %2").arg(indir).arg(qcoutdir);
									n->WriteLog(n->SystemCommand(systemstring));
									msgs << "Copying QC data from [" + indir + "/qa] to [" + qcoutdir + "]";

									/* write the series info to a text file */
									QString seriesfile = outdir + "seriesinfo.txt";
									QFile f(seriesfile);
									if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
										QTextStream fs(&f);
										QSqlQuery q;
										q.prepare("select * from mr_series where mrseries_id = :seriesid");
										q.bindValue(":seriesid",seriesid);
										n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
										if (q.size() > 0) {
											QSqlRecord r(q.record());
											QStringList fields;
											for (int v = 0; v < r.count(); ++v)
												fields << r.fieldName(v);

											q.first();
											foreach (QString field, fields) {
												fs << QString("%1: %2").arg(field).arg(q.value(field).toString());
											}
										}
										f.close();
									}
									else {
										msgs << "Unable to create series info file [" + seriesfile + "]";
									}
								}
								else {
									QString tmpdir = n->cfg["tmpdir"] + "/" + n->GenerateRandomString(10);
									QString m1;
									if (n->MakePath(tmpdir, m1)) {
										msgs << "Created tmpdir [" + tmpdir + "]";
										QString m2;
										int numfilesconv(0), numfilesrenamed(0);
										if (!n->ConvertDicom(filetype, indir, tmpdir, gzip, uid, QString("%1").arg(studynum), QString("%1").arg(seriesnum), datatype, numfilesconv, numfilesrenamed, m2))
											msgs << "Error converting files [" + m2 + "]";
										n->WriteLog("About to copy files from " + tmpdir + " to " + outdir);
										QString systemstring = "rsync " + tmpdir + "/* " + outdir + "/";
										n->WriteLog(n->SystemCommand(systemstring));
										n->WriteLog("Done copying files...");
										QString m3;
										if (!n->RemoveDir(tmpdir, m3))
											msgs << "Error [" + m3 + "] while removing path [" + tmpdir + "]";
										msgs << "Converted DICOM/parrec data into " + filetype + " using tmpdir [" + tmpdir + "]. Final directory [" + outdir + "]";
									}
									else
										msgs << "Error [" + m1 + "]. Unable to create path [" + tmpdir + "]";
								}
							}
							else {
								seriesstatus = "error";
								exportstatus = "error";
								n->WriteLog("ERROR [" + indir + "] is empty");
								msgs << "Directory [" + indir + "] is empty";
								statusmessage = "Directory [" + indir + "] is empty. Data missing from disk";
							}
						}
						else {
							seriesstatus = "error";
							exportstatus = "error";
							n->WriteLog("ERROR indir [" + indir + "] does not exist");
							msgs << "Directory [" + indir + "] does not exist";
							statusmessage = "Directory [" + indir + "] does not exist. Data missing from disk";
						}
					}
					else {
						n->WriteLog("numfiles is 0");
						msgs << "Series contains 0 files";
					}
				}
                else {
                    n->WriteLog("Imaging data not selected for download");
                }

				/* export the beh data */
				if (downloadbeh) {
					if (behdirexists) {
						QString m;
						if (n->MakePath(behoutdir, m)) {
							QString systemstring = "cp -R " + behindir + "/* " + behoutdir;
							n->WriteLog(n->SystemCommand(systemstring, true));
							systemstring = "chmod -Rf 777 " + behoutdir;
							n->WriteLog(n->SystemCommand(systemstring, true));
							msgs << "Copying behavioral data from [" + behindir + "] to [" + behoutdir + "]";
						}
						else
							msgs << "Error [" + m + "] while creating path [" + behoutdir + "]";
					}
					else {
						n->WriteLog("WARNING behindir [" + behindir + "] does not exist");
						msgs << "Directory [" + behindir + "] does not exist";
					}
				}
				else {
					n->WriteLog("Not downloading beh data");
					msgs << "Not downloading beh data\n";
				}

				/* copy the QC data */
				if (downloadqc) {
					if (qcdirexists) {
						QString m;
						if (n->MakePath(qcoutdir, m)) {
							QString systemstring = "cp -R " + qcindir + "/* " + qcoutdir;
							n->WriteLog(n->SystemCommand(systemstring, true));
							systemstring = "chmod -Rf 777 " + qcoutdir;
							n->WriteLog(n->SystemCommand(systemstring, true));
							msgs << "Copying QC data from [" + qcindir + "] to [" + qcoutdir + "]";
						}
						else
							msgs << "Error [" + m + "] while creating path [" + behoutdir + "]";
					}
					else {
						seriesstatus = "error";
						exportstatus = "error";
						n->WriteLog("ERROR qcindir [" + qcindir + "] does not exist");
						msgs << "Directory [" + qcindir + "] does not exist";
						statusmessage = "Directory [" + qcindir + "] does not exist";
					}
				}

				/* give full permissions to the files that were downloaded */
				if (exporttype == "nfs") {
					QString systemstring = "chmod -Rf 777 " + rootoutdir;
					n->WriteLog(n->SystemCommand(systemstring, true));
				}

				if (filetype == "dicom")
					n->AnonymizeDir(outdir,anonlevel,"Anonymous","Anonymous");

				SetExportSeriesStatus(exportseriesid,seriesstatus,statusmessage);
				msgs << QString("Series [%1%2-%3 (%4)] complete").arg(uid).arg(studynum).arg(seriesnum).arg(seriesdesc);

				laststudynum = studynum;
			}
		}
	}

	/* extra steps for web download */
	if (exporttype == "web") {
		QString zipfile = QString("%1/NIDB-%2.zip").arg(n->cfg["webdownloaddir"]).arg(exportid);
		QString outdir;
		n->WriteLog("Final zip file will be [" + zipfile + "]");
		n->WriteLog("tmpexportdir: [" + tmpexportdir + "]");
		outdir = tmpexportdir;

		QString pwd = QDir::currentPath();
		n->WriteLog("Current directory is [" + pwd + "], changing directory to [" + outdir + "]");
		QDir d;
		if (d.exists(outdir)) {
			QString systemstring;
			QDir::setCurrent(outdir);
			if (QFile::exists(zipfile))
				systemstring = "zip -1grv " + zipfile + " .";
			else
				systemstring = "zip -1rv " + zipfile + " .";
			n->WriteLog("Beginning zipping...");
			n->WriteLog(n->SystemCommand(systemstring, true));
			n->WriteLog("Finished zipping...");
			n->WriteLog("Changing directory to [" + pwd + "]");
			QDir::setCurrent(pwd);
		}
		else {
			n->WriteLog("outdir [" + outdir + "] does not exist");
		}

		/* delete the tmp dir, if it exists */
		if (d.exists(tmpexportdir)) {
			n->WriteLog("Temporary export dir [" + tmpexportdir + "] exists and will be deleted");
			QString m;
			if (!n->RemoveDir(tmpexportdir, m))
				msgs << "Error [" + m + "] removing directory [" + tmpexportdir + "]";
		}
		QFile file;
		if (file.exists(zipfile))
			msgs << "Created .zip file [" + zipfile + "]";
		else
			msgs << "Unable to create [" + zipfile + "]";
	}

	/* extra steps for publicdownload */
	if (exporttype == "publicdownload") {

		QSqlQuery q;
		q.prepare("select * from public_downloads where pd_id = :publicdownloadid");
		q.bindValue(":publicdownloadid",publicdownloadid);
		n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
		if (q.size() > 0) {
			q.first();
			int expiredays = q.value("pd_expiredays").toInt();

			QString filename = QString("NiDB-%1.zip").arg(exportid);
			QString zipfile = n->cfg["webdownloaddir"] + "/" + filename;
			QString outdir = tmpexportdir;

			QString pwd = QDir::currentPath();
			n->WriteLog("Current directory is [" + pwd + "], changing directory to [" + outdir + "]");
			QDir d;
			if (d.exists(outdir)) {
				QString systemstring;
				QDir::setCurrent(outdir);
				if (QFile::exists(zipfile))
					systemstring = "zip -1grq " + zipfile + " .";
				else
					systemstring = "zip -1rq " + zipfile + " .";
				n->WriteLog(n->SystemCommand(systemstring, true));
				n->WriteLog("Changing directory to [" + pwd + "]");
				QDir::setCurrent(pwd);
				systemstring = "unzip -vl " + zipfile;
				QString filecontents = n->SystemCommand(systemstring, false);
				QStringList lines = filecontents.split("\n");
				QString lastline = lines.last().trimmed();
				n->WriteLog(QString("Last line of [%1] %2").arg(systemstring).arg(lastline));
                QStringList parts = lastline.split(QRegExp("\\s+"), Qt::SkipEmptyParts); /* split on whitespace */
				int unzippedsize(0);
				int zippedsize(0);
				if (parts.size() >= 2) {
					unzippedsize = parts[0].toInt();
					zippedsize = parts[1].toInt();
				}

				QSqlQuery q2;
				q2.prepare("update public_downloads set pd_createdate = now(), pd_expiredate = date_add(now(), interval :expiredays day), pd_zippedsize = :zippedsize, pd_unzippedsize = :unzippedsize, pd_filename = :filename, pd_filecontents = :filecontents, pd_key = upper(sha1(now())), pd_status = 'preparing' where pd_id = :publicdownloadid");
				q2.bindValue(":expiredays",expiredays);
				q2.bindValue(":zippedsize",zippedsize);
				q2.bindValue(":unzippedsize",unzippedsize);
				q2.bindValue(":filename",filename);
				q2.bindValue(":filecontents",filecontents);
				q2.bindValue(":publicdownloadid",publicdownloadid);
				n->SQLQuery(q2, __FUNCTION__, __FILE__, __LINE__);
			}
			else {
				exportstatus = "error";
				n->WriteLog("ERROR directory [" + outdir + "] does not exist");
				msgs << "Outdir [" + zipfile + "] does not exist";
			}

			if (QFile::exists(zipfile)) {
				msgs << "Created .zip file [" + zipfile + "]";
			}
			else {
				exportstatus = "error";
				n->WriteLog("ERROR unable to create zip file [" + zipfile + "]");
				msgs << "Unable to create [" + zipfile + "]";
			}
		}
		else {
			/* public downloadid not found */
		}
	}

	n->WriteLog("Leaving ExportLocal()...");

	msg = msgs.join("\n");

	return 1;
}


/* ---------------------------------------------------------- */
/* --------- ExportNDAR ------------------------------------- */
/* ---------------------------------------------------------- */
bool moduleExport::ExportNDAR(int exportid, bool csvonly, QString &exportstatus, QString &msg) {

	n->WriteLog("Entering ExportNDAR()...");
	exportstatus = "complete";

	QStringList msgs;
	if (!GetExportSeriesList(exportid)) {
		msg = "Unable to get a series list";
		return false;
	}

	QString rootoutdir = n->cfg["ftpdir"] + "/NiDB-NDAR-" + n->CreateLogDate();
	QString headerfile = rootoutdir + "/ndar.csv";

	msgs << "ExportNDAR() rootoutdir [" + rootoutdir + "]";
	msgs << "ExportNDAR() .csv header file [" + headerfile + "]";

	QString m;
	if (n->MakePath(rootoutdir, m)) {
		msgs << "ExportNDAR() " + n->WriteLog("Created rootoutdir [" + rootoutdir + "]");
	}
	else {
		exportstatus = "error";
		msgs << "ExportNDAR() " + n->WriteLog("ERROR [" + m + "] unable to create rootoutdir [" + rootoutdir + "]");
		return false;
	}

	QString systemstring;
	/* iterate through the UIDs */
	for(QMap<QString, QMap<int, QMap<int, QMap<QString, QString>>>>::iterator a = s.begin(); a != s.end(); ++a) {
		QString uid = a.key();

		/* iterate through the studynums */
		for(QMap<int, QMap<int, QMap<QString, QString>>>::iterator b = s[uid].begin(); b != s[uid].end(); ++b) {
			int studynum = b.key();

			/* iterate through the seriesnums */
			for(QMap<int, QMap<QString, QString>>::iterator c = s[uid][studynum].begin(); c != s[uid][studynum].end(); ++c) {
				int seriesnum = c.key();

				int exportseriesid = s[uid][studynum][seriesnum]["exportseriesid"].toInt();
				SetExportSeriesStatus(exportseriesid, "processing");

				QString seriesstatus = "complete";
				QString statusmessage;

				int seriesid = s[uid][studynum][seriesnum]["seriesid"].toInt();
				QString primaryaltuid = s[uid][studynum][seriesnum]["primaryaltuid"];
				QString altuids = s[uid][studynum][seriesnum]["altuids"];
				QString projectname = s[uid][studynum][seriesnum]["projectname"];
				QString studytype = s[uid][studynum][seriesnum]["studytype"];
				QString studyaltid = s[uid][studynum][seriesnum]["studyaltid"];
				QString modality = s[uid][studynum][seriesnum]["modality"];
				int numfilesbeh = s[uid][studynum][seriesnum]["numfilesbeh"].toInt();
				QString datatype = s[uid][studynum][seriesnum]["datatype"];
				QString indir = s[uid][studynum][seriesnum]["datadir"];
				QString behindir = s[uid][studynum][seriesnum]["behdir"];
				QString qcindir = s[uid][studynum][seriesnum]["qcdir"];
				bool datadirexists = s[uid][studynum][seriesnum]["datadirexists"].toInt();

				QStringList logs;

				if (datadirexists) {
					WriteNDARHeader(headerfile, modality, logs);
					msgs << logs;

					QString behzipfile;
					QString behdesc;

					/* write the header, find out if the data is valid and should copied to the output */
					bool validData = WriteNDARSeries(headerfile, QString("%1-%2-%3.zip").arg(uid).arg(studynum).arg(seriesnum), behzipfile, behdesc, seriesid, modality, indir, logs);
					msgs << logs;

					if (!csvonly && validData) {
						QString tmpdir = n->cfg["tmpdir"] + "/" + n->GenerateRandomString(10);
						m = "";
						if (n->MakePath(tmpdir, m)) {
							QString systemstring;
							if ((modality == "mr") && (datatype == "dicom")) {
								systemstring = "find " + indir + " -iname '*.dcm' -exec cp {} " + tmpdir + " \\;";
								msgs << "ExportNDAR() " + n->WriteLog(n->SystemCommand(systemstring, true));
								n->AnonymizeDir(tmpdir,2,"","");
							}
							else if ((modality == "mr") && (datatype == "parrec")) {
								systemstring = "find " + indir + " -iname '*.par' -exec cp {} " + tmpdir + " \\;";
								msgs << "ExportNDAR() " + n->WriteLog(n->SystemCommand(systemstring, true));
								systemstring = "find " + indir + " -iname '*.rec' -exec cp {} " + tmpdir + " \\;";
								msgs << "ExportNDAR() " + n->WriteLog(n->SystemCommand(systemstring, true));
							}
							else {
								systemstring = "rsync " + indir + "/* " + tmpdir + "/";
								msgs << "ExportNDAR() " + n->WriteLog(n->SystemCommand(systemstring, true));
							}

							/* zip the data to the output directory */
							QString zipfile = QString("%1/%2-%3-%4.zip").arg(rootoutdir).arg(uid).arg(studynum).arg(seriesnum);
							systemstring = "zip -vjrq1 " + zipfile + " " + tmpdir;
							msgs << "ExportNDAR() " + n->WriteLog(n->SystemCommand(systemstring, true));
							msgs << "ExportNDAR() " + n->WriteLog("Done zipping image files...");

							/* create a behavioral data zip file if there is beh data */
							if (numfilesbeh > 0) {
								behzipfile = QString("%1-%2-%3-beh.zip").arg(uid).arg(studynum).arg(seriesnum);
								systemstring = QString("zip -vjrq1 %1/%2 %3").arg(rootoutdir).arg(behzipfile).arg(behindir);
								msgs << "ExportNDAR() " + n->WriteLog(n->SystemCommand(systemstring, true));
								msgs << "ExportNDAR() " + n->WriteLog("Done zipping beh files...");

								behdesc = "Behavioral/design data file";
							}
							if (modality == "mr") {
								if (!n->RemoveDir(tmpdir,m))
									msgs << "ExportNDAR() Unable to remove tmpdir [" + tmpdir + "] because [" + m + "]";
							}
						}
						else {
							seriesstatus = "error";
							statusmessage = "Unable to create tmpdir [" + tmpdir + "] because [" + m + "]";
							msgs << "ExportNDAR() " + statusmessage;
						}
					}

				}
				else {
					seriesstatus = "error";
					statusmessage = "Data directory [" + indir + "] does not exist";
					msgs << "ExportNDAR() Data directory does not exist. Unable to export data from [" + indir + "]\n";
				}
				SetExportSeriesStatus(exportseriesid,seriesstatus,statusmessage);
			}
		}
	}

	n->WriteLog("Leaving ExportNDAR()...");

	msg = msgs.join("\n");

	return true;
}


/* ---------------------------------------------------------- */
/* --------- ExportBIDS ------------------------------------- */
/* ---------------------------------------------------------- */
bool moduleExport::ExportBIDS(int exportid, QString bidsreadme, QString &exportstatus, QString &msg) {
	n->WriteLog("Entering ExportBIDS()...");

	exportstatus = "complete";

	QStringList msgs;
	if (!GetExportSeriesList(exportid)) {
		msg = "Unable to get a series list";
		return false;
	}

	QString rootoutdir = n->cfg["ftpdir"] + "/NiDB-BIDS-" + n->CreateLogDate();

	QString m;
	if (n->MakePath(rootoutdir, m)) {
		n->WriteLog("Created rootoutdir [" + rootoutdir + "]");
	}
	else {
		exportstatus = "error";
		msg = n->WriteLog("ERROR [" + m + "] unable to create rootoutdir [" + rootoutdir + "]");
		return false;
	}

	int i = 1; /* the subject counter */
	/* iterate through the UIDs */
	for(QMap<QString, QMap<int, QMap<int, QMap<QString, QString>>>>::iterator a = s.begin(); a != s.end(); ++a) {
		QString uid = a.key();
		int j = 1; /* the session (study) counter */

		/* iterate through the studynums */
		for(QMap<int, QMap<int, QMap<QString, QString>>>::iterator b = s[uid].begin(); b != s[uid].end(); ++b) {
			int studynum = b.key();

			/* iterate through the seriesnums */
			for(QMap<int, QMap<QString, QString>>::iterator c = s[uid][studynum].begin(); c != s[uid][studynum].end(); ++c) {
				int seriesnum = c.key();

				int exportseriesid = s[uid][studynum][seriesnum]["exportseriesid"].toInt();
				SetExportSeriesStatus(exportseriesid, "processing");

				QString seriesstatus = "complete";
				QString statusmessage;

				//int seriesid = s[uid][studynum][seriesnum]["seriesid"].toInt();
				//int subjectid = s[uid][studynum][seriesnum]["subjectid"].toInt();
				QString primaryaltuid = s[uid][studynum][seriesnum]["primaryaltuid"];
				QString altuids = s[uid][studynum][seriesnum]["altuids"];
				QString projectname = s[uid][studynum][seriesnum]["projectname"];
				//int studyid = s[uid][studynum][seriesnum]["studyid"].toInt();
				QString studytype = s[uid][studynum][seriesnum]["studytype"];
				QString studyaltid = s[uid][studynum][seriesnum]["studyaltid"];
				QString modality = s[uid][studynum][seriesnum]["modality"];
				//double seriessize = s[uid][studynum][seriesnum]["seriessize"].toDouble();
				QString seriesdesc = s[uid][studynum][seriesnum]["seriesdesc"];
				QString seriesaltdesc = s[uid][studynum][seriesnum]["seriesaltdesc"].trimmed();
				QString datatype = s[uid][studynum][seriesnum]["datatype"];
				QString indir = s[uid][studynum][seriesnum]["datadir"];
				QString behindir = s[uid][studynum][seriesnum]["behdir"];
				QString qcindir = s[uid][studynum][seriesnum]["qcdir"];
				bool datadirexists = s[uid][studynum][seriesnum]["datadirexists"].toInt();
				bool behdirexists = s[uid][studynum][seriesnum]["behdirexists"].toInt();
				//bool qcdirexists = s[uid][studynum][seriesnum]["qcdirexists"].toInt();
				bool datadirempty = s[uid][studynum][seriesnum]["datadirempty"].toInt();
				//bool behdirempty = s[uid][studynum][seriesnum]["behdirempty"].toInt();
				//bool qcdirempty = s[uid][studynum][seriesnum]["qcdirempty"].toInt();

				/* create the subject identifier */
				QString subjectdir = QString("subj%1").arg(i, 4, 10, QChar('0'));

				/* create the session (study) identifier */
				QString sessiondir = QString("sess%1").arg(j, 4, 10, QChar('0'));

				/* determine the datatype (what BIDS calls the 'modality') */
				QString seriesdir;
				if (seriesaltdesc == "") {
					seriesdir = seriesdesc;
				}
				else {
					seriesdir = seriesaltdesc;
				}
				/* remove any non-alphanumeric characters */
				seriesdir.replace(QRegularExpression("[^a-zA-Z0-9_-]"),"_");

				QString outdir = QString("%1/%2/%3/%4").arg(rootoutdir).arg(subjectdir).arg(sessiondir).arg(seriesdir);

				QString m;
				if (n->MakePath(outdir, m)) {
					n->WriteLog("Created outdir [" + outdir + "]");
				}
				else {
					exportstatus = "error";
					n->WriteLog("ERROR [" + m + "] unable to create outdir [" + outdir + "]");
					msg = "Unable to create output directory [" + outdir + "]";
					return false;
				}

				if (datadirexists) {
					if (!datadirempty) {
						QString tmpdir = n->cfg["tmpdir"] + "/" + n->GenerateRandomString(10);
						QString m;
						if (n->MakePath(tmpdir, m)) {

							int numfilesconv(0), numfilesrenamed(0);
							if (!n->ConvertDicom("bids", indir, tmpdir, 1, subjectdir, sessiondir, seriesdir, datatype, numfilesconv, numfilesrenamed, m))
								msgs << "Error converting files [" + m + "]";

							n->WriteLog("About to copy files from " + tmpdir + " to " + outdir);
							QString systemstring = "rsync " + tmpdir + "/* " + outdir + "/";
							n->WriteLog(n->SystemCommand(systemstring));
							n->WriteLog("Done copying files...");
							n->RemoveDir(tmpdir,m);
						}
						else {
							n->WriteLog("Unable to create directory");
						}
					}
					else {
						seriesstatus = "error";
						exportstatus = "error";
						n->WriteLog("ERROR [" + indir + "] is empty");
						msgs << "Directory [" + indir + "] is empty";
					}
				}
				else {
					seriesstatus = "error";
					exportstatus = "error";
					n->WriteLog("ERROR indir [" + indir + "] does not exist");
					msgs << "Directory [" + indir + "] does not exist";
				}

				/* copy the beh data */
				if (behdirexists) {
					QString systemstring;
					systemstring = "cp -R " + behindir + "/* " + outdir;
					n->WriteLog(n->SystemCommand(systemstring, true));
					systemstring = "chmod -Rf 777 " + outdir;
					n->WriteLog(n->SystemCommand(systemstring, true));
				}

				SetExportSeriesStatus(exportseriesid, seriesstatus);
			}
		}
	}

	/* write the readme file */
	QString readmefilename = rootoutdir + "/README";
	QFile f(readmefilename);
	if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QTextStream fs(&f);
		fs << bidsreadme;
		f.close();
	}
	else
		msgs << "Unable to create BIDS README file [" + readmefilename + "]";

	msg = msgs.join("\n");
	n->WriteLog("Leaving ExportBIDS()...");
	return true;
}


/* ---------------------------------------------------------- */
/* --------- ExportToRemoteNiDB ----------------------------- */
/* ---------------------------------------------------------- */
bool moduleExport::ExportToRemoteNiDB(int exportid, remoteNiDBConnection &conn, QString &exportstatus, QString &msg) {

	/* check to see if the remote server is reachable ... */
	QString systemstring = "curl -sSf " + conn.server;
	QString serverResponse = n->SystemCommand(systemstring, false);
	if ((serverResponse == "") || (serverResponse.contains("Could not resolve host",Qt::CaseInsensitive))) {
		msg = n->WriteLog("ERROR: Unable to access remote NiDB server [" + conn.server + "]. Received error [" + serverResponse + "]");
		return false;
	}
	/* ... and if our credentials work and we can start a transaction on it */
	int transactionid = StartRemoteNiDBTransaction(conn.server, conn.username, conn.password);
	if (transactionid < 0) {
		msg = n->WriteLog(QString("ERROR: Invalid transaction ID [%1] received from [%2]").arg(transactionid).arg(conn.server));
		return false;
	}

	/* update the exports table with the transaction ID */
	QSqlQuery q;
	q.prepare("update exports set remotenidb_transactionid = :transactionid where export_id = :exportid");
	q.bindValue(":transactionid",transactionid);
	q.bindValue(":exportid",exportid);
	n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);

	QString tmpexportdir = n->cfg["tmpdir"] + "/" + n->GenerateRandomString(20);

	QStringList msgs;
	if (!GetExportSeriesList(exportid)) {
		msg = "Unable to get a series list";
		return false;
	}

	exportstatus = "complete";
	/* iterate through the UIDs */
	for(QMap<QString, QMap<int, QMap<int, QMap<QString, QString>>>>::iterator a = s.begin(); a != s.end(); ++a) {
		QString uid = a.key();
		/* iterate through the studynums */
		for(QMap<int, QMap<int, QMap<QString, QString>>>::iterator b = s[uid].begin(); b != s[uid].end(); ++b) {
			int studynum = b.key();
			/* iterate through the seriesnums */
			for(QMap<int, QMap<QString, QString>>::iterator c = s[uid][studynum].begin(); c != s[uid][studynum].end(); ++c) {
				int seriesnum = c.key();

				int exportseriesid = s[uid][studynum][seriesnum]["exportseriesid"].toInt();
				SetExportSeriesStatus(exportseriesid, "processing");

				QString seriesstatus = "complete";
				QString statusmessage;

				//int seriesid = s[uid][studynum][seriesnum]["seriesid"].toInt();
				//int subjectid = s[uid][studynum][seriesnum]["subjectid"].toInt();
				QString primaryaltuid = s[uid][studynum][seriesnum]["primaryaltuid"];
				QString altuids = s[uid][studynum][seriesnum]["altuids"];
				QString projectname = s[uid][studynum][seriesnum]["projectname"];
				//int studyid = s[uid][studynum][seriesnum]["studyid"].toInt();
				QString studytype = s[uid][studynum][seriesnum]["studytype"];
				QString studyaltid = s[uid][studynum][seriesnum]["studyaltid"];
				QString modality = s[uid][studynum][seriesnum]["modality"];
				//double seriessize = s[uid][studynum][seriesnum]["seriessize"].toDouble();
				QString seriesnotes = s[uid][studynum][seriesnum]["seriesnotes"];
				QString seriesdesc = s[uid][studynum][seriesnum]["seriesdesc"];
				QString datatype = s[uid][studynum][seriesnum]["datatype"];
				QString indir = s[uid][studynum][seriesnum]["datadir"];
				QString behindir = s[uid][studynum][seriesnum]["behdir"];
				QString qcindir = s[uid][studynum][seriesnum]["qcdir"];
				bool datadirexists = s[uid][studynum][seriesnum]["datadirexists"].toInt();
				bool behdirexists = s[uid][studynum][seriesnum]["behdirexists"].toInt();
				//bool qcdirexists = s[uid][studynum][seriesnum]["qcdirexists"].toInt();
				bool datadirempty = s[uid][studynum][seriesnum]["datadirempty"].toInt();
				bool behdirempty = s[uid][studynum][seriesnum]["behdirempty"].toInt();
				//bool qcdirempty = s[uid][studynum][seriesnum]["qcdirempty"].toInt();

				/* remove any non-alphanumeric characters */
				seriesnotes.replace(QRegExp("[^a-zA-Z0-9 _-]", Qt::CaseInsensitive), "");

				msgs << QString("uid [%1] indir [%2] datadirexists [%3]").arg(uid).arg(indir).arg(datadirexists);
				if (datadirexists) {
					if (!datadirempty) {
						// --------------- Send to remote NiDB site --------------------------

						int numfails = 0;
						int error = 1;
						QString results;
						QString systemstring;

						int numretry = 5;
						if (n->cfg["numretry"].toInt() > 0)
							numretry = n->cfg["numretry"].toInt();

						while ((error == 1) && (numfails < numretry)) {
							QString indir = QString("%1/%2/%3/%4/%5").arg(n->cfg["archivedir"]).arg(uid).arg(studynum).arg(seriesnum).arg(datatype);
							QString behindir = QString("%1/%2/%3/%4/beh").arg(n->cfg["archivedir"]).arg(uid).arg(studynum).arg(seriesnum);
							QString tmpdir = n->cfg["tmpdir"] + "/" + n->GenerateRandomString(10);
							QString tmpzip = n->cfg["tmpdir"] + "/" + n->GenerateRandomString(12) + ".tar.gz";
							QString tmpzipdir = n->cfg["tmpdir"] + "/" + n->GenerateRandomString(12);
							QString m;
							if (!n->MakePath(tmpdir, m)) { msgs << "ERROR in creating tmpdir [" + tmpdir + "]"; continue; }
							if (!n->MakePath(tmpzipdir + "/beh", m)) { msgs << "ERROR in creating tmpzipdir/beh [" + tmpzipdir + "/beh]"; continue; }

							/* copy all the files from the data directory into a tmp directory */
							systemstring = "rsync " + indir + "/* " + tmpdir + "/";
							n->WriteLog(n->SystemCommand(systemstring));
							if (datatype == "dicom")
								n->AnonymizeDir(tmpdir,4,"Anonymous","0000-00-00");

							/* get the list of DICOM files */
							QStringList dcmfiles = n->FindAllFiles(tmpdir, "*");
							int numdcms = dcmfiles.size();
							n->WriteLog(QString("Found [%1] dcmfiles").arg(numdcms));

							if (numdcms < 1)
								n->WriteLog("************* ERROR - Didn't find any DICOM files!!!! *************");

							/* get the list of beh files */
							QStringList behfiles;
							if (behdirexists && !behdirempty)
								QStringList behfiles = n->FindAllFiles(behindir, "*");

							/* build the cURL string to send the actual data */
							systemstring = QString("curl -gs -F 'action=UploadDICOM' -F 'u=%1' -F 'p=%2' -F 'transactionid=%3' -F 'instanceid=%4' -F 'projectid=%5' -F 'siteid=%6' -F 'dataformat=%7' -F 'modality=%8' -F 'seriesnotes=%9' -F 'altuids=%10' -F 'seriesnum=%11' ").arg(conn.username).arg(conn.password).arg(transactionid).arg(conn.instanceid).arg(conn.projectid).arg(conn.siteid).arg(datatype).arg(modality).arg(seriesnotes).arg(altuids).arg(seriesnum);
							int c = 0;
							foreach (QString f, dcmfiles) {
								c++;
								QString systemstringA = QString("cp -v '%1' %2/").arg(f).arg(tmpzipdir);
								QString output = n->SystemCommand(systemstringA);
								if ((output == "") || output.contains("error", Qt::CaseInsensitive))
									n->WriteLog(output);
							}

							if (behdirexists && !behdirempty) {
								c = 0;
								foreach(QString f, behfiles) {
									c++;
									QString systemstringA = QString("cp '%1/%2' %3/beh/").arg(behindir).arg(f).arg(tmpzipdir);
									QString res = n->SystemCommand(systemstringA, false);
									if (res != "") {
										n->WriteLog(systemstringA + " (" + res + ")");
									}
								}
							}

							/* send the zip and send file */
							QString systemstringB = QString("GZIP=-1; tar -czf %2 --warning=no-timestamp -C %1 .; chmod -v 777 %2").arg(tmpzipdir).arg(tmpzip);
							n->WriteLog(n->SystemCommand(systemstringB));

							/* get size before sending */
							QFile zf(tmpzip);
							double zipsize = zf.size();
							double starttime = QDateTime::currentMSecsSinceEpoch();

							/* get file MD5 before sending */
							QString zipmd5 = n->GetFileChecksum(tmpzip, QCryptographicHash::Md5).toHex();

							systemstring += "-F 'files[]=@" + tmpzip + "' ";
							systemstring += conn.server + "/api.php";
							QString results = n->SystemCommand(systemstring, false);
							n->WriteLog("Ran [" + systemstring + "] output (" + results + ")");
							double elapsedtime = QDateTime::currentMSecsSinceEpoch() - starttime + 0.0000001; // to avoid a divide by zero!
							double MBps = zipsize/elapsedtime/1000.0;
							QString speedmsg = QString("%1 bytes transferred in %2s - Speed: %3 MB/s").arg(zipsize).arg(elapsedtime*1000.0).arg(QString::number(MBps, 'g', 2));
							n->WriteLog(speedmsg);
							msgs << speedmsg;

							QStringList parts = results.split(",");
							if (parts[0].trimmed() == "SUCCESS") {
								/* a file was received by the remote NiDB server, now check the return md5 */
								if (parts[1].trimmed().toUpper() == zipmd5.toUpper()) {
									seriesstatus = "complete";
									n->WriteLog("Upload success: MD5 match");
									msgs << "Successfully sent data to [" + conn.server + "]";
									error = 0;
								}
								else {
									seriesstatus = exportstatus = "error";
									msgs << n->WriteLog("Upload fail: MD5 non-match");
									error = 1;
									numfails++;
								}
							}
							else {
								seriesstatus = exportstatus = "error";
								msgs << n->WriteLog("Upload fail: got message [" + results + "]");
								error = 1;
								numfails++;
							}
						}
					}
					else {
						seriesstatus = exportstatus = "error";
						msgs << n->WriteLog("ERROR indir [" + indir + "] is empty");
					}
				}
				else {
					seriesstatus = exportstatus = "error";
					msgs << n->WriteLog("ERROR indir [" + indir + "] does not exist");
				}
				SetExportSeriesStatus(exportseriesid, seriesstatus);
				msgs << n->WriteLog(QString("Series [%1%2-%3 (%4)] complete").arg(uid).arg(studynum).arg(seriesnum).arg(seriesdesc));
			}
		}
	}

	EndRemoteNiDBTransaction(transactionid, conn.server, conn.username, conn.password);

	n->WriteLog("Leaving ExportToRemoteNiDB()...");

	return true;
}


/* ---------------------------------------------------------- */
/* --------- ExportToRemoteFTP ------------------------------ */
/* ---------------------------------------------------------- */
bool moduleExport::ExportToRemoteFTP(int exportid, QString remoteftpusername, QString remoteftppassword, QString remoteftpserver, int remoteftpport, QString remoteftppath, QString &exportstatus, QString &msg) {

	/* was once implemented in Perl version, but was never used. Now not implemented */

	n->WriteLog(QString("ExportToRemoteFTP(%1, %2, %3, %4, %5, %6, %7, %8) called").arg(exportid).arg(remoteftpusername).arg(remoteftppassword).arg(remoteftpserver).arg(remoteftpport).arg(remoteftppath).arg(exportstatus).arg(msg));

	return true;
}


/* ---------------------------------------------------------- */
/* --------- WriteNDARHeader -------------------------------- */
/* ---------------------------------------------------------- */
bool moduleExport::WriteNDARHeader(QString file, QString modality, QStringList &log) {

	QFile f(file);
	if (f.exists())
		return true;

	if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {

		modality = modality.toLower();

		QTextStream fs(&f);
		if (modality.toLower() == "mr") {
			fs << "image,3\n";
			fs << "subjectkey,src_subject_id,interview_date,interview_age,gender,comments_misc,image_file,image_thumbnail_file,image_description,image_file_format,image_modality,scanner_manufacturer_pd,scanner_type_pd,scanner_software_versions_pd,magnetic_field_strength,mri_repetition_time_pd,mri_echo_time_pd,flip_angle,acquisition_matrix,mri_field_of_view_pd,patient_position,photomet_interpret,receive_coil,transmit_coil,transformation_performed,transformation_type,image_history,image_num_dimensions,image_extent1,image_extent2,image_extent3,image_extent4,extent4_type,image_extent5,extent5_type,image_unit1,image_unit2,image_unit3,image_unit4,image_unit5,image_resolution1,image_resolution2,image_resolution3,image_resolution4,image_resolution5,image_slice_thickness,image_orientation,qc_outcome,qc_description,qc_fail_quest_reason,decay_correction,frame_end_times,frame_end_unit,frame_start_times,frame_start_unit,pet_isotope,pet_tracer,time_diff_inject_to_image,time_diff_units,scan_type,scan_object,data_file2,data_file2_type,experiment_description,experiment_id,pulse_seq,slice_acquisition,software_preproc,study,week,slice_timing,bvek_bval_files\n";
		}
		if (modality == "eeg") {
			fs << "eeg_sub_files,1\n";
			fs << "subjectkey,src_subject_id,interview_date,interview_age,gender,comments_misc,capused,ofc,experiment_id,experiment_notes,experiment_terminated,experiment_validity,data_behavioralperformance_acc,data_behavioralperformance_rt,data_file1,data_file1_type,data_file2,data_file2_type,data_file3,data_file3_type,data_file4,data_file4_type,data_includedtrials,data_validity\n";
		}
		if (modality == "et") {
			fs << "et_subject_experiment,1\n";
			fs << "subjectkey,src_subject_id,interview_date,interview_age,gender,phenotype,experiment_id,comments_misc,experiment_validity,experiment_notes,experiment_terminated,expcond_validity,expcond_notes,data_file1,data_file1_type,data_file2,data_file2_type,data_file3,data_file3_type,data_file4,data_file4_type\n";
		}

		f.close();

		log << "WriteNDARHeader() " + n->WriteLog("Wrote header to NDAR .csv file [" + file + "]");

		return true;
	}
	else
		return false;
}


/* ---------------------------------------------------------- */
/* --------- WriteNDARSeries -------------------------------- */
/* ---------------------------------------------------------- */
bool moduleExport::WriteNDARSeries(QString file, QString imagefile, QString behfile, QString behdesc, int seriesid, QString modality, QString indir, QStringList &log) {

	/* get the information on the subject and series */
	QSqlQuery q;
	q.prepare(QString("select *, date_format(study_datetime,'%m/%d/%Y') 'study_datetime', TIMESTAMPDIFF(MONTH, birthdate, study_datetime) 'ageatscan' from %1_series a left join studies b on a.study_id = b.study_id left join enrollment c on b.enrollment_id = c.enrollment_id left join subjects d on c.subject_id = d.subject_id left join projects e on c.project_id = e.project_id where %1series_id = :seriesid").arg(modality));
	q.bindValue(":seriesid", seriesid);
	n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);

	if (q.size() > 0) {
		while (q.next()) {
			int subjectid = q.value("subject_id").toInt();
			int enrollmentid = q.value("enrollment_id").toInt();
			QString guid = q.value("guid").toString().trimmed();
			QString seriesdatetime = q.value("series_datetime").toString().trimmed();
			double seriestr = q.value("series_tr").toDouble();
			double serieste = q.value("series_te").toDouble();
			double seriesflip = q.value("series_flip").toDouble();
			QString seriesprotocol = q.value("series_protocol").toString().trimmed();
			QString seriessequence = q.value("series_sequencename").toString().trimmed();
			QString seriesnotes = q.value("series_notes").toString().trimmed();
			QString imagetype = q.value("image_type").toString().trimmed();
			QString imagecomments = q.value("image_comments").toString().trimmed();
			double seriesspacingx = q.value("series_spacingx").toDouble();
			double seriesspacingy = q.value("series_spacingy").toDouble();
			double seriesspacingz = q.value("series_spacingz").toDouble();
			double seriesfieldstrength = q.value("series_fieldstrength").toDouble();
			int imgrows = q.value("img_rows").toInt();
			int imgcols = q.value("img_cols").toInt();
			int imgslices = q.value("img_slices").toInt();
			QString datatype = q.value("data_type").toString().trimmed().toUpper();
			QString studydatetime = q.value("study_datetime").toString().trimmed();
			QString birthdate = q.value("birthdate").toString().trimmed();
			QString gender = q.value("gender").toString().trimmed();
			QString uid = q.value("uid").toString().trimmed();
			double ageatscan = q.value("ageatscan").toDouble();
			double studyageatscan = q.value("study_ageatscan").toDouble();
			QString seriesdesc = q.value("series_desc").toString().trimmed();
			int boldreps = q.value("bold_reps").toInt();
			int projectid = q.value("project_id").toInt();

			/* skip this subject entirely if the GUID is blank... we can't submit to NDAR/RDoC if there is no GUID */
			if (guid == "") {
				log << "WriteNDARSeries() " + n->WriteLog("GUID was blank for subject [" + uid + "], skipping writing any data for this subject");
				return false;
			}

			int numdim;
			if (boldreps > 1)
				numdim = 4;
			else
				numdim = 3;

			if (modality == "mr")
				modality = "mri";

			modality = modality.toUpper();

			if (imgrows < 1) imgrows = 1;
			if (imgcols < 1) imgcols = 1;
			if (imgslices < 1) imgslices = 1;

			/* fix ages that are stored in months, although they shouldn't be... */
			if ((studyageatscan > 0) && (studyageatscan < 120))
				ageatscan = studyageatscan * 12.0;

			QString srcsubjectid;
			QString altuid = n->GetPrimaryAlternateUID(subjectid, enrollmentid);
			if (altuid == "") {
				srcsubjectid = uid;
			}
			else {
				srcsubjectid = altuid;
			}

			/* open the file appending */
			QFile f(file);
			if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
				log << "WriteNDARSeries() " + n->WriteLog("Could not open NDAR .csv output file [" + file + "]");
				continue;
			}
			QTextStream fs(&f);

			/* create the modality specific line for the csv */
			if (modality == "MRI") {

				QString Manufacturer;
				QString ProtocolName;
				QString PercentPhaseFieldOfView;
				QString PatientPosition;
				QString AcquisitionMatrix;
				QString SoftwareVersion;
				QString PhotometricInterpretation;
				QString ManufacturersModelName;
				QString TransmitCoilName;
				QString SequenceName;

				if (datatype == "DICOM") {
					/* get some DICOM specific tags from the first file in the series */
					QString dcmfile;
					QString m;
					if (!n->FindFirstFile(indir, "*.dcm",dcmfile,m)) {
						log << "WriteNDARSeries() " + n->WriteLog("Unable to find any DICOM files in [" + indir + "]");
						return false;
					}

					gdcm::Reader r;
					r.SetFileName(dcmfile.toStdString().c_str());
					if (!r.Read()) {
						/* could not read the first dicom file... */
						log << "WriteNDARSeries() " + n->WriteLog("Could not read DICOM file [" + dcmfile + "]");
						return false;
					}

					r.Read();
					gdcm::StringFilter sf;
					sf = gdcm::StringFilter();
					sf.SetFile(r.GetFile());

					Manufacturer = QString(sf.ToString(gdcm::Tag(0x0008,0x0070)).c_str()).trimmed(); /* Manufacturer */
					ProtocolName = QString(sf.ToString(gdcm::Tag(0x0018,0x1030)).c_str()).trimmed(); /* ProtocolName */
					PercentPhaseFieldOfView = QString(sf.ToString(gdcm::Tag(0x0018,0x0094)).c_str()).trimmed(); /* PercentPhaseFieldOfView */
					PatientPosition = QString(sf.ToString(gdcm::Tag(0x0018,0x5100)).c_str()).trimmed(); /* PatientPosition */
					AcquisitionMatrix = QString(sf.ToString(gdcm::Tag(0x0008,0x0060)).c_str()).trimmed(); /* modality */
					SoftwareVersion = QString(sf.ToString(gdcm::Tag(0x0008,0x0060)).c_str()).trimmed(); /* modality */
					PhotometricInterpretation = QString(sf.ToString(gdcm::Tag(0x0008,0x0060)).c_str()).trimmed(); /* modality */
					ManufacturersModelName = QString(sf.ToString(gdcm::Tag(0x0008,0x0070)).c_str()).trimmed(); /* ManufacturersModelName */
					TransmitCoilName = QString(sf.ToString(gdcm::Tag(0x0008,0x0070)).c_str()).trimmed(); /* TransmitCoilName */
					SequenceName = QString(sf.ToString(gdcm::Tag(0x0008,0x0070)).c_str()).trimmed(); /* SequenceName */
				}

				/* clean up the tags */
				if (Manufacturer == "") Manufacturer = "Unknown";
				if (PatientPosition == "") PatientPosition = "Unknown";
				if (SoftwareVersion == "") SoftwareVersion = "Unknown";
				if (PhotometricInterpretation == "") PhotometricInterpretation = "RGB";
				if (ManufacturersModelName == "") ManufacturersModelName = "Unknown";
				if (TransmitCoilName == "") TransmitCoilName = "Unknown";

				/* figure out the scan type (T1,T2,DTI,fMRI) */
				QString scantype = "MR structural (T1)";
				if ((boldreps > 1) || (seriessequence.contains("epfid2d1")))
					scantype = "fMRI";
				if (seriesdesc.contains("perfusion",Qt::CaseInsensitive) && seriessequence.contains("ep2d_perf_tra", Qt::CaseInsensitive))
					scantype = "MR diffusion";

				if (seriesdesc.contains("dti",Qt::CaseInsensitive) || seriesdesc.contains("dwi",Qt::CaseInsensitive))
					scantype = "MR diffusion";
				if (seriesdesc.contains("T2"))
					scantype = "MR structural (T2)";

				/* build the aquisition matrix */
				if (AcquisitionMatrix.trimmed() == "") {
					AcquisitionMatrix = "0 0 0 0";
				}

				QString FOV = "0x0";
				QStringList AcqParts = AcquisitionMatrix.split(" ");
				if (AcqParts.size() >= 4)
					FOV = QString("%1mm x %2mm").arg((AcqParts[0].toDouble() * seriesspacingx * PercentPhaseFieldOfView.toDouble())/100.0).arg((AcqParts[3].toDouble() * seriesspacingy * PercentPhaseFieldOfView.toDouble())/100.0);

				QString str;
				QTextStream(&str) << guid << "," << srcsubjectid << "," << studydatetime << "," << ageatscan << "," << gender << "," << imagetype << "," << imagefile << ",," << seriesdesc << "," << datatype << "," << modality << "," << Manufacturer << "," << ManufacturersModelName << "," << SoftwareVersion << "," << seriesfieldstrength << "," << seriestr << "," << serieste << "," << seriesflip << "," << AcquisitionMatrix << "," << FOV << "," << PatientPosition << "," << PhotometricInterpretation << ",," << TransmitCoilName << ",No,,," << numdim << "," << imgcols << "," << imgrows << "," << imgslices << "," << boldreps << ",timeseries,,,Millimeters,Millimeters,Millimeters,Milliseconds,," << seriesspacingx << "," << seriesspacingy << "," << seriesspacingz << "," << seriestr << ",," << seriesspacingz << ",Axial,,,,,,,,,,,,," << scantype << ",Live," << behfile << "," << behdesc << "," << ProtocolName << ",," << seriessequence << ",1,,,0,Yes,Yes\n";

				fs << str;
			}
			else if (modality == "EEG") {
				int expid = 0;

				QString sp = seriesprotocol.toLower();

				/* NDAR */
				if (sp.contains("domino",Qt::CaseInsensitive)) expid = 115;
				if (sp.contains("SPMain",Qt::CaseInsensitive)) expid = 114;
				if (sp.contains("SPGender", Qt::CaseInsensitive)) expid = 114;
				if (sp.contains("HNumber",Qt::CaseInsensitive)) expid = 113;
				if (sp.contains("HPain", Qt::CaseInsensitive)) expid = 113;

				if ((sp == "gating") || (sp == "gating2") || (sp == "gating3")) expid = 530;
				if ((sp == "resteyesopen") || (sp == "rest") || (sp == "rest - eyes open")) expid = 528;
				if ((sp == "resteyesclosed") || (sp == "rest - eyes closed")) expid = 556;
				if ((sp == "oddball") || (sp == "oddball - beh data")) expid = 529;

				/* PARDIP */
				if ((projectid == 173) || (projectid == 174) || (projectid == 176)) {
					if (sp == "auditory steady state") expid = 538;
					else if (sp == "rmr") expid = 575;
					else if (sp == "rest - eyes open") expid = 531;
					else if (sp == "pro-saccade") expid = 566;
					else if (sp == "anti-saccade") expid = 569;
					else if (sp == "iaps") expid = 537;
					else if (sp == "visual steady state") expid = 539;
					else if (sp == "oddball") expid = 532;
					else if (sp == "gating") expid = 536;
				}

				/* BSNIP2 */
				if ((projectid == 185) || (projectid == 187) || (projectid == 191) || (projectid == 192) || (projectid == 194)) {
					if (sp == "rest - eyes open") expid = 549;
					else if (sp == "rmr") expid = 587;
					else if (sp == "anti-saccade") expid = 559;
					else if (sp == "pro-saccade") expid = 558;
					else if (sp == "iaps") expid = 582;
					else if (sp == "visual steady state") expid = 584;
					else if (sp == "auditory steady state") expid = 583;
					else if (sp == "oddball") expid = 550;
					else if (sp == "gating") expid = 581;
				}

				QString str;
				QTextStream(&str) << guid << "," << uid << "," << studydatetime << "," << ageatscan << "," << gender << "," << seriesprotocol << ",,," << expid <<",\"" << seriesnotes << "\",,,,," << imagefile << ",,,,,,,,,\n";
				fs << str;
			}
			else if (modality == "ET") {
				int expid = 0;
				QString str;
				QTextStream(&str) << guid << "," << uid << "," << studydatetime << "," << ageatscan << "," << gender << ",Unknown," << expid << "," << seriesprotocol << ",,\"" << seriesnotes << "\",,,," << imagefile << ",Eyetracking,,,,,,\n";
				fs << str;
			}
			else {
				log << "WriteNDARSeries() " + n->WriteLog("Unknown modality [" + modality + "]. Nothing to written to file [" + file + "].");
			}

			f.close();
		}
	}
	else {
		log << "WriteNDARSeries() " + n->WriteLog(QString("No rows found for this series... [%1, %2, %3, %4, %5, %6, %7] ").arg(file).arg(imagefile).arg(behfile).arg(behdesc).arg(seriesid).arg(modality).arg(indir));
	}

	return true;
}


/* ---------------------------------------------------------- */
/* --------- StartRemoteNiDBTransaction --------------------- */
/* ---------------------------------------------------------- */
int moduleExport::StartRemoteNiDBTransaction(QString remotenidbserver, QString remotenidbusername, QString remotenidbpassword) {

	int ret = -1;
	/* build a cURL string to start the transaction */
	QString systemstring = QString("curl -gs -F 'action=startTransaction' -F 'u=%1' -F 'p=%2' %3/api.php").arg(remotenidbusername).arg(remotenidbpassword).arg(remotenidbserver);
	QString str = n->SystemCommand(systemstring, false).simplified();

	bool ok;
	int t = str.toLong(&ok);
	if (ok)
		ret = t;

	n->WriteLog(QString("Remote NiDB transactionID: [%1]").arg(t));

    return ret;
}


/* ---------------------------------------------------------- */
/* --------- EndRemoteNiDBTransaction ----------------------- */
/* ---------------------------------------------------------- */
void moduleExport::EndRemoteNiDBTransaction(int tid, QString remotenidbserver, QString remotenidbusername, QString remotenidbpassword) {

	/* build a cURL string to end the transaction */
	QString systemstring = QString ("curl -gs -F 'action=endTransaction' -F 'u=%1' -F 'p=%2' -F 'transactionid=%3' %4/api.php").arg(remotenidbusername).arg(remotenidbpassword).arg(tid).arg(remotenidbserver);
	n->WriteLog(n->SystemCommand(systemstring));
}
