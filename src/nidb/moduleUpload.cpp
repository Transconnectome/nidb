/* ------------------------------------------------------------------------------
  NIDB moduleUpload.cpp
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

#include "moduleUpload.h"
#include <QSqlQuery>

/* ---------------------------------------------------------- */
/* --------- moduleUpload ----------------------------------- */
/* ---------------------------------------------------------- */
moduleUpload::moduleUpload(nidb *a)
{
    n = a;
}


/* ---------------------------------------------------------- */
/* --------- ~moduleUpload ---------------------------------- */
/* ---------------------------------------------------------- */
moduleUpload::~moduleUpload()
{

}


/* ---------------------------------------------------------- */
/* --------- Run -------------------------------------------- */
/* ---------------------------------------------------------- */
int moduleUpload::Run() {
    n->WriteLog("Entering the upload module");

    QSqlQuery q;
    int ret(0);

    /* get list of uploads that are marked as uploadcomplete, with the upload details */
    q.prepare("select * from uploads where status = 'uploadcomplete'");
    n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
    if (q.size() > 0) {
        while (q.next()) {
            int upload_id = q.value("upload_id").toInt();
            QString upload_source = q.value("upload_source").toString();
            QString upload_datadir = q.value("upload_datadir").toString();
            //int upload_destprojectid = q.value("upload_destprojectid").toInt();
            QString upload_modality = q.value("upload_modality").toString();
            //bool upload_guessmodality = q.value("upload_guessmodality").toBool();
            QString upload_subjectcriteria = q.value("upload_subjectcriteria").toString();
            QString upload_studycriteria = q.value("upload_studycriteria").toString();
            QString upload_seriescriteria = q.value("upload_seriescriteria").toString();

            /* create a multilevel hash [subject][study][series][files] */
            QMap<QString, QMap<QString, QMap<QString, QStringList>>> fs;

            /* create the path for the upload data */
            QString uploadpath = QString("%1/%2").arg(n->cfg["uploadstagingdir"]).arg(upload_id);

            /* create temporary directory in uploadstagingdir */
            QString m;
            if (!n->MakePath(uploadpath, m)) {
                n->WriteLog("Error creating directory [" + uploadpath + "]  with message [" + m + "]");
                continue;
            }

            /* copy in files from uploadtmp or nfs to the uploadstagingdir */
            QString systemstring = QString("cp -ruv %1/* %2/").arg(upload_datadir).arg(uploadpath);
            n->SystemCommand(systemstring);

            /* get information about the uploaded data from the uploadstagingdir (before unzipping any zip files) */
            int c;
            qint64 b;
            n->GetDirSizeAndFileCount(uploadpath, c, b, true);
            n->WriteLog(QString("Upload directory [%1] contains [%2] files, and is [%3] bytes in size.").arg(uploadpath).arg(c).arg(b));

            /* unzip any files in the uploadstagingdir (3 passes) */
            n->WriteLog(n->UnzipDirectory(uploadpath, true));
            n->WriteLog(n->UnzipDirectory(uploadpath, true));
            n->WriteLog(n->UnzipDirectory(uploadpath, true));

            /* get information about the uploaded data from the uploadstagingdir (after unzipping any zip files) */
            c = 0;
            b = 0;
            n->GetDirSizeAndFileCount(uploadpath, c, b, true);
            n->WriteLog(QString("After 3 passes of UNZIPPING files, upload directory [%1] now contains [%2] files, and is [%3] bytes in size.").arg(uploadpath).arg(c).arg(b));

            /* get list of all files, and iterate through all of the files */
            QStringList files = n->FindAllFiles(uploadpath, "*", true);
            foreach (QString f, files) {
                QString subject, study, series;

                /* get the file info */
                QHash<QString, QString> tags;
                if (n->GetImageFileTags(f, tags)) {
                    if (tags["Modality"] == upload_modality) {

                        /* subject matching criteria */
                        if (upload_subjectcriteria == "patientid")
                            subject = tags["PatientID"];
                        else if (upload_subjectcriteria == "namesexdob")
                            subject = tags["PatientName"] + "|" + tags["PatientSex"] + "|" + tags["PatientBirthDate"];
                        else
                            n->WriteLog("Unspecified subject criteria [" + upload_subjectcriteria + "]");

                        /* study matching criteria */
                        if (upload_studycriteria == "modalitystudydate")
                            study = tags["Modality"] + "|" + tags["StudyDateTime"];
                        else if (upload_studycriteria == "studyuid")
                            study = tags["StudyInstanceUID"];
                        else
                            n->WriteLog("Unspecified study criteria [" + upload_studycriteria + "]");

                        /* series matching criteria */
                        if (upload_seriescriteria == "seriesnumber")
                            series = tags["SeriesNumber"];
                        else if (upload_seriescriteria == "seriesdate")
                            series = tags["SeriesDate"] + "|" + tags["SeriesTime"];
                        else if (upload_seriescriteria == "seriesuid")
                            series = tags["SeriesInstanceUID"];
                        else
                            n->WriteLog("Unspecified series criteria [" + upload_seriescriteria + "]");

                        /* store the file in the appropriate group */
                        fs[subject][study][series].append(f);
                    }
                    else {
                        n->WriteLog("Valid file [" + f + "] but not the modality we're looking for [" + tags["Modality"] + "]");
                        fs["nonmatch"]["nonmatch"]["nonmatch"].append(f);
                    }
                }
                else {
                    /* the file is not readable */
                    fs["unreadable"]["unreadable"]["unreadable"].append(f);
                    n->WriteLog("Unable to read file [" + f + "]");
                }
            }


            /* ---------- iterate through the subjects ---------- */
            for(QMap<QString, QMap<QString, QMap<QString, QStringList>>>::iterator a = fs.begin(); a != fs.end(); ++a) {
                QString subject = a.key();

                /* get the uploadsubject_id */
                int subjectid(0);

                if (upload_subjectcriteria == "patientid") {
                    /* get subjectid by PatientID field */
                    QString PatientID = subject;

                    QSqlQuery q;
                    /* check if the subjectid exists ... */
                    q.prepare("select uploadsubject_id from upload_subjects where upload_id = :uploadid and uploadsubject_patientid = :patientid");
                    q.bindValue(":uploadid", upload_id);
                    q.bindValue(":patientid", PatientID);
                    n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                    if (q.size() > 0) {
                        q.first();
                        subjectid = q.value("uploadsubject_id").toInt();
                    }
                    else {
                        /* ... otherwise create a new subject */
                        q.prepare("insert into upload_subjects (upload_id, uploadsubject_patientid) values (:uploadid, :patientid)");
                        q.bindValue(":uploadid", upload_id);
                        q.bindValue(":patientid", PatientID);
                        n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                        subjectid = q.lastInsertId().toInt();
                    }
                }
                else if (upload_subjectcriteria == "namesexdob") {
                    /* get subjectid by PatientName/PatientSex/PatientBirthDate */
                    QStringList parts = subject.split("|");
                    QString PatientName = parts[0];
                    QString PatientSex = parts[1];
                    QString PatientBirthDate = parts[2];

                    QSqlQuery q;
                    /* check if the subjectid already exists ... */
                    q.prepare("select uploadsubject_id from upload_subjects where upload_id = :uploadid and uploadsubject_name = :patientname and uploadsubject_dob = :patientdob and uploadsubject_sex = :patientsex");
                    q.bindValue(":uploadid", upload_id);
                    q.bindValue(":patientname", PatientName);
                    q.bindValue(":patientdob", PatientBirthDate);
                    q.bindValue(":patientsex", PatientSex);
                    n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                    if (q.size() > 0) {
                        q.first();
                        subjectid = q.value("uploadsubject_id").toInt();
                    }
                    else {
                        /* ... otherwise create a new subject */
                        q.prepare("insert into upload_subjects (upload_id, uploadsubject_patientname, uploadsubject_patientdob, uploadsubject_patientsex) values (:uploadid, :patientname, :patientdob, :patientsex)");
                        q.bindValue(":uploadid", upload_id);
                        q.bindValue(":patientname", PatientName);
                        q.bindValue(":patientdob", PatientBirthDate);
                        q.bindValue(":patientsex", PatientSex);
                        n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                        subjectid = q.lastInsertId().toInt();
                    }
                }
                else
                    n->WriteLog("Unspecified subject criteria [" + upload_subjectcriteria + "]. That's weird it would show up here...");

                /* ---------- iterate through the studies ---------- */
                for(QMap<QString, QMap<QString, QStringList>>::iterator b = fs[subject].begin(); b != fs[subject].end(); ++b) {
                    QString study = b.key();

                    /* get the uploadstudy_id */
                    int studyid(0);

                    if (upload_studycriteria == "modalitystudydate") {
                        /* get studyid from Modality/StudyDateTime fields */
                        QStringList parts = study.split("|");
                        QString Modality = parts[0];
                        QString StudyDateTime = parts[1];

                        QSqlQuery q;
                        /* check if the studyid exists ... */
                        q.prepare("select uploadstudy_id from upload_studies where uploadsubject_id = :subjectid and uploadstudy_date = :studydatetime and uploadstudy_modality = :modality");
                        q.bindValue(":subjectid", subjectid);
                        q.bindValue(":studydatetime", StudyDateTime);
                        q.bindValue(":modality", Modality);
                        n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                        if (q.size() > 0) {
                            q.first();
                            studyid = q.value("uploadstudy_id").toInt();
                        }
                        else {
                            /* ... otherwise create a new study */
                            q.prepare("insert into upload_studies (uploadsubject_id, uploadstudy_date, uploadstudy_modality) values (:subjectid, :studydatetime, :modality)");
                            q.bindValue(":subjectid", subjectid);
                            q.bindValue(":patientid", StudyDateTime);
                            q.bindValue(":modality", Modality);
                            n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                            studyid = q.lastInsertId().toInt();
                        }
                    }
                    else if (upload_studycriteria == "studyuid") {
                        /* get studyid using StudyInstanceUID field */
                        QString StudyInstanceUID = study;

                        QSqlQuery q;
                        /* check if the studyid already exists ... */
                        q.prepare("select uploadstudy_id from upload_studies where uploadsubject_id = :subjectid and uploadstudy_instanceuid = :studyinstanceuid");
                        q.bindValue(":subjectid", subjectid);
                        q.bindValue(":studyinstanceuid", StudyInstanceUID);
                        n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                        if (q.size() > 0) {
                            q.first();
                            studyid = q.value("uploadstudy_id").toInt();
                        }
                        else {
                            /* ... otherwise create a new study */
                            q.prepare("insert into upload_studies (uploadsubject_id, uploadstudy_instanceuid) values (:subjectid, :studyinstanceuid)");
                            q.bindValue(":subjectid", subjectid);
                            q.bindValue(":studyinstanceuid", StudyInstanceUID);
                            n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                            studyid = q.lastInsertId().toInt();
                        }
                    }
                    else
                        n->WriteLog("Unspecified study criteria [" + upload_studycriteria + "]. That's weird it would show up here...");

                    /* ---------- iterate through the series ---------- */
                    for(QMap<QString, QStringList>::iterator c = fs[subject][study].begin(); c != fs[subject][study].end(); ++c) {
                        QString series = c.key();

                        /* get uploadseries_id */
                        int seriesid(0);

                        if (upload_seriescriteria == "seriesnumber") {
                            /* get seriesid from SeriesNumber field */
                            QString SeriesNumber = series;

                            QSqlQuery q;
                            /* check if the studyid exists ... */
                            q.prepare("select uploadseries_id from upload_series where uploadstudy_id = :studyid and uploadseries_num = :seriesnum");
                            q.bindValue(":studyid", studyid);
                            q.bindValue(":seriesnum", SeriesNumber);
                            n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                            if (q.size() > 0) {
                                q.first();
                                seriesid = q.value("uploadseries_id").toInt();
                            }
                            else {
                                /* ... otherwise create a new series */
                                q.prepare("insert into upload_series (uploadstudy_id, uploadseries_num) values (:studyid, :seriesnum)");
                                q.bindValue(":studyid", studyid);
                                q.bindValue(":seriesnum", SeriesNumber);
                                n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                                seriesid = q.lastInsertId().toInt();
                            }
                        }
                        else if (upload_seriescriteria == "seriesdate") {
                            /* get seriesid using SeriesDateTime field */
                            QString SeriesDateTime = series;

                            QSqlQuery q;
                            /* check if the seriesid already exists ... */
                            q.prepare("select uploadseries_id from upload_series where uploadstudy_id = :studyid and uploadseries_date = :seriesdatetime");
                            q.bindValue(":studyid", studyid);
                            q.bindValue(":seriesdatetime", SeriesDateTime);
                            n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                            if (q.size() > 0) {
                                q.first();
                                seriesid = q.value("uploadseries_id").toInt();
                            }
                            else {
                                /* ... otherwise create a new series */
                                q.prepare("insert into upload_studies (uploadstudy_id, uploadseries_date) values (:studyid, :seriesdatetime)");
                                q.bindValue(":studyid", studyid);
                                q.bindValue(":seriesdatetime", SeriesDateTime);
                                n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                                seriesid = q.lastInsertId().toInt();
                            }
                        }
                        else if (upload_seriescriteria == "seriesuid") {
                            /* get seriesid using SeriesInstanceUID field */
                            QString SeriesInstanceUID = series;

                            QSqlQuery q;
                            /* check if the seriesid already exists ... */
                            q.prepare("select uploadseries_id from upload_series where uploadstudy_id = :studyid and uploadseries_instanceuid = :seriesinstanceuid");
                            q.bindValue(":studyid", studyid);
                            q.bindValue(":seriesinstanceuid", SeriesInstanceUID);
                            n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                            if (q.size() > 0) {
                                q.first();
                                seriesid = q.value("uploadseries_id").toInt();
                            }
                            else {
                                /* ... otherwise create a new series */
                                q.prepare("insert into upload_series (uploadstudy_id, uploadseries_instanceuid) values (:studyid, :seriesinstanceuid)");
                                q.bindValue(":studyid", studyid);
                                q.bindValue(":seriesinstanceuid", SeriesInstanceUID);
                                n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                                seriesid = q.lastInsertId().toInt();
                            }
                        }
                        else {
                            n->WriteLog("Unspecified study criteria [" + upload_studycriteria + "]. That's weird it would show up here...");
                        }

                        QStringList files = fs[subject][study][series];

                        /* we've arrived at a series, so let's put it into the database */
                        /* get tags from first file in the list to populate the subject/study/series info not included in the criteria matching */
                        QHash<QString, QString> tags;
                        if (n->GetImageFileTags(files[0], tags)) {

                            /* don't overwrite the tags in the databse that were used to group the subject/study/series */

                            /* update subject details */
                            if (upload_subjectcriteria == "patientid") {
                                /* update all subject details except PatientID */
                                q.prepare("update ignore upload_subjects set uploadsubject_patientname = :name, uploadsubject_patientsex = :sex, uploadsubject_patientdob = :dob where uploadsubject_id = :subjectid");
                                q.bindValue(":name", tags["PatientName"]);
                                q.bindValue(":sex", tags["PatientSex"]);
                                q.bindValue(":dob", tags["PatientBirthDate"]);
                                q.bindValue(":subjectid", subjectid);
                                n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                            }
                            else if (upload_subjectcriteria == "namesexdob") {
                                /* update all subject details except PatientName/Sex/BirthDate */
                                q.prepare("update ignore upload_subjects set uploadsubject_patientid = :patientid where uploadsubject_id = :subjectid");
                                q.bindValue(":name", tags["PatientID"]);
                                q.bindValue(":subjectid", subjectid);
                                n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                            }
                            else n->WriteLog("Unspecified subject criteria [" + upload_subjectcriteria + "]");


                            /* update study details */
                            if (upload_studycriteria == "modalitystudydate") {
                                /* update all study details except Modality/StudyDateTime */
                                q.prepare("update upload_studies set uploadstudy_desc = :desc, uploadstudy_datatype = :datatype, uploadstudy_equipment = :equipment, uploadstudy_operator = :operator where uploadstudy_id = :studyid");
                                q.bindValue(":desc", tags["StudyDescription"]);
                                q.bindValue(":datatype", tags["FileType"]);
                                q.bindValue(":equipment", tags["Manufacturer"] + " " + tags["ManufacturerModelName"]);
                                q.bindValue(":operator", tags["OperatorsName"]);
                                q.bindValue(":studyinstanceuid", tags["StudyInstanceUID"]);
                                q.bindValue(":studyid", studyid);
                                n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                            }
                            else if (upload_studycriteria == "studyuid") {
                                /* update all study details except StudyInstanceUID */
                                q.prepare("update upload_studies set uploadstudy_desc = :desc, uploadstudy_date = :datetime, uploadstudy_modality = :modality, uploadstudy_datatype = :datatype, uploadstudy_equipment = :equipment, uploadstudy_operator = :operator where uploadstudy_id = :studyid");
                                q.bindValue(":desc", tags["StudyDescription"]);
                                q.bindValue(":datetime", tags["StudyDateTime"]);
                                q.bindValue(":modality", tags["Modality"]);
                                q.bindValue(":datatype", tags["FileType"]);
                                q.bindValue(":equipment", tags["Manufacturer"] + " " + tags["ManufacturerModelName"]);
                                q.bindValue(":operator", tags["OperatorsName"]);
                                q.bindValue(":studyinstanceuid", tags["StudyInstanceUID"]);
                                q.bindValue(":studyid", studyid);
                                n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                            }
                            else n->WriteLog("Unspecified study criteria [" + upload_studycriteria + "]");


                            /* update series details */
                            if (upload_seriescriteria == "seriesnumber") {
                                /* update all series details except SeriesNumber */
                                q.prepare("update ignore upload_series set uploadseries_desc = :desc, uploadseries_protocol = :protocol, uploadseries_date = :date, uploadseries_numfiles = :numfiles, uploadseries_tr = :tr, uploadseries_te = :te, uploadseries_slicespacing = :slicespacing, uploadseries_slicethickness = :slicethickness, uploadseries_rows = :rows, uploadseries_cols = :cols, uploadseries_instanceuid = :seriesinstanceuid, uploadseries_files = :files where uploadseries_id = :seriesid");
                                q.bindValue(":desc", tags["SeriesDescription"]);
                                q.bindValue(":protocol", tags["ProtocolName"]);
                                q.bindValue(":date", tags["SeriesDateTime"]);
                                q.bindValue(":numfiles", files.size());
                                q.bindValue(":tr", tags["RepetitionTime"]);
                                q.bindValue(":te", tags["EchoTime"]);
                                q.bindValue(":slicespacing", tags["SpacingBetweenSlices"]);
                                q.bindValue(":slicethickness", tags["SliceThickness"]);
                                q.bindValue(":rows", tags["Rows"]);
                                q.bindValue(":cols", tags["Columns"]);
                                q.bindValue(":seriesinstanceuid", tags["SeriesInstanceUID"]);
                                q.bindValue(":files", files.join(","));
                                q.bindValue(":seriesid", seriesid);
                                n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                            }
                            else if (upload_seriescriteria == "seriesdate") {
                                /* update all series details except SeriesDateTime */
                                q.prepare("update ignore upload_series set uploadseries_desc = :desc, uploadseries_protocol = :protocol, uploadseries_num = :num, uploadseries_numfiles = :numfiles, uploadseries_tr = :tr, uploadseries_te = :te, uploadseries_slicespacing = :slicespacing, uploadseries_slicethickness = :slicethickness, uploadseries_rows = :rows, uploadseries_cols = :cols, uploadseries_instanceuid = :seriesinstanceuid, uploadseries_files = :files where uploadseries_id = :seriesid");
                                q.bindValue(":desc", tags["SeriesDescription"]);
                                q.bindValue(":protocol", tags["ProtocolName"]);
                                q.bindValue(":num", tags["SeriesNumber"]);
                                q.bindValue(":numfiles", files.size());
                                q.bindValue(":tr", tags["RepetitionTime"]);
                                q.bindValue(":te", tags["EchoTime"]);
                                q.bindValue(":slicespacing", tags["SpacingBetweenSlices"]);
                                q.bindValue(":slicethickness", tags["SliceThickness"]);
                                q.bindValue(":rows", tags["Rows"]);
                                q.bindValue(":cols", tags["Columns"]);
                                q.bindValue(":seriesinstanceuid", tags["SeriesInstanceUID"]);
                                q.bindValue(":files", files.join(","));
                                q.bindValue(":seriesid", seriesid);
                                n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                            }
                            else if (upload_seriescriteria == "seriesuid") {
                                /* update all series details except SeriesInstanceUID */
                                q.prepare("update ignore upload_series set uploadseries_desc = :desc, uploadseries_protocol = :protocol, uploadseries_num = :num, uploadseries_date = :date, uploadseries_numfiles = :numfiles, uploadseries_tr = :tr, uploadseries_te = :te, uploadseries_slicespacing = :slicespacing, uploadseries_slicethickness = :slicethickness, uploadseries_rows = :rows, uploadseries_cols = :cols, uploadseries_files = :files where uploadseries_id = :seriesid");
                                q.bindValue(":desc", tags["SeriesDescription"]);
                                q.bindValue(":protocol", tags["ProtocolName"]);
                                q.bindValue(":date", tags["SeriesDateTime"]);
                                q.bindValue(":num", tags["SeriesNumber"]);
                                q.bindValue(":numfiles", files.size());
                                q.bindValue(":tr", tags["RepetitionTime"]);
                                q.bindValue(":te", tags["EchoTime"]);
                                q.bindValue(":slicespacing", tags["SpacingBetweenSlices"]);
                                q.bindValue(":slicethickness", tags["SliceThickness"]);
                                q.bindValue(":rows", tags["Rows"]);
                                q.bindValue(":cols", tags["Columns"]);
                                q.bindValue(":files", files.join(","));
                                q.bindValue(":seriesid", seriesid);
                                n->SQLQuery(q, __FUNCTION__, __FILE__, __LINE__);
                            }
                            else n->WriteLog("Unspecified series criteria [" + upload_seriescriteria + "]");

                        }
                        else {
                            n->WriteLog("Error reading file [" + files[0] + "]. That's weird it would show up here...");
                        }
                    }
                }
            }

        } /* end while */
    }

    n->WriteLog("Leaving the upload module");
    return ret;
}
