/*
 Copyright 2023 Google LLC

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

#include <QDialog>
#include "capture_service/device_mgr.h"

#pragma once

#include <QComboBox>
#include <QIdentityProxyModel>
#include <QStandardItemModel>
// Forward declarations
class QLabel;
class QHBoxLayout;
class QPlainTextEdit;
class QPushButton;
class QVBoxLayout;
class QComboBox;
class QStandardItemModel;
class QLineEdit;
class QSortFilterProxyModel;
class TraceDialog;

class FileSelectDialog : public QDialog
{
    Q_OBJECT

public:
    FileSelectDialog(TraceDialog *trace_dig);
    void SetModel(QStandardItemModel *model)
    {
        m_pkg_model = model;
        m_pkg_box->setModel(m_pkg_model);
        m_pkg_box->setCurrentIndex(-1);
    }

    void SetPackageList(std::vector<std::string> pkg_list) { m_pkg_list = std::move(pkg_list); }

private slots:
    void OnPackageSelected(const QString &);

    // signals:
    //     void PackageSelected(const QString &);

private:
    QLabel                  *m_pkg_label;
    QStandardItemModel      *m_pkg_model;
    QComboBox               *m_pkg_box;
    std::vector<std::string> m_pkg_list;
    std::string              m_cur_pkg;
    TraceDialog             *m_trace_dig;
};

class TraceDialog : public QDialog
{
    Q_OBJECT

public:
    TraceDialog(QWidget *parent = 0);
    ~TraceDialog();
    void UpdateDeviceList();
    void Cleanup() { Dive::GetDeviceManager().RemoveDevice(); }
    void OnPackageSelected(const QString &);

private slots:
    void OnDeviceSelected(const QString &);
    void OnStartClicked();
    void OnTraceClicked();
    void OnOpenClicked();

signals:
    void TraceAvailable(const QString &);

private:
    void ShowErrorMessage(const std::string &err_msg);

    QHBoxLayout      *m_file_layout;
    QLabel           *m_file_label;
    QPushButton      *m_open_button;
    QLineEdit        *m_cmd_input_box;
    FileSelectDialog *m_file_model;

    QHBoxLayout        *m_dev_layout;
    QLabel             *m_dev_label;
    QStandardItemModel *m_dev_model;
    QComboBox          *m_dev_box;

    QStandardItemModel *m_pkg_model;

    QLabel             *m_app_type_label;
    QStandardItemModel *m_app_type_model;
    QComboBox          *m_app_type_box;

    QPushButton *m_capture_button;
    QPushButton *m_run_button;
    QHBoxLayout *m_button_layout;

    QVBoxLayout                  *m_main_layout;
    std::vector<Dive::DeviceInfo> m_devices;
    std::string                   m_cur_dev;
    std::vector<std::string>      m_pkg_list;
    std::string                   m_cur_pkg;

    FileSelectDialog *m_file_dig;
};
