/***************************************************************************
 *   Author aka. crispina                 *
 *   crispinalan@gmail.com                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("LXQt Organizer"));
    //setup databse
    dbm.openDatabase();
    dbm.createDatebaseTables();
    QMainWindow::centralWidget()->layout()->setContentsMargins(0, 0, 0, 0);

    //setup calendar
    ui->calendarWidget->setGridVisible(false); //no grid
    ui->actionCalendar_Grid->setChecked(false);
    ui->calendarWidget->setVerticalHeaderFormat(ui->calendarWidget->NoVerticalHeader);
    //setup tableview
    ui->tableView->horizontalHeader()->setVisible(true);
    ui->tableView->horizontalHeader()->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->verticalHeader()->setVisible(false);

    ui->tableViewContacts->horizontalHeader()->setVisible(true);
    ui->tableViewContacts->horizontalHeader()->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableViewContacts->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableViewContacts->horizontalHeader()->setStretchLastSection(true);
    ui->tableViewContacts->verticalHeader()->setVisible(false);

    this->selectedDate=QDate::currentDate();
    ui->labelSchedule->setText(QLatin1String("Appointments for ")+selectedDate.toString());

    //setup status bar
    statusTimeLabel= new QLabel();
    statusTimeLabel->setText(QTime::currentTime().toString(Qt::SystemLocaleDate));
    statusTimeLabel->setText(QTime::currentTime().toString(QStringLiteral("hh:mm")));
    statusDateLabel = new QLabel(this);
    statusDateLabel->setText(QDate::currentDate().toString());
    ui->statusBar->addPermanentWidget(statusDateLabel);
    ui->statusBar->addPermanentWidget(statusTimeLabel);
    ui->statusBar->showMessage(QStringLiteral("Hello. Watch for notifications here! Double click items to edit"),5000);

    //setup timer
    timer = new QTimer();
    connect(timer,SIGNAL(timeout()),this, SLOT(timerUpdateSlot()));
    timer->start(1000); //check for reminders every 30000ms or30 seconds
    //pause timer when reminder QTimer::stop()

    //setup models
    //day appointments
    dayModel = new AppointmentModel(); //stores appointments for a day
    proxyModel = new ProxyModel; //needed for time sorting
    //contact model
    contactModel = new ContactModel; //stores all contacts
    proxyModelContacts= new ProxyModelContacts;  
    reminderModel = new ReminderModel(); //stores reminders
    birthdayModel= new BirthdayModel(); //stores birthdays
    birthdayReminderModel= new BirthdayModel();

    setCalendarOptions();

    //Load Lists
    LoadContactsListFromDatabase();
    DisplayContactsOnTableView();
    LoadBirthdayListFromDatabase(); //birthdayList is loaded
    LoadAppointmentsListFromDatabase();    
    LoadReminderListFromDatabase();

    //Display
    DisplayAppointmentsForDate(this->selectedDate);
    DisplayBirthdaysForDate(this->selectedDate);
    showAllBirthdaysOnCalendar();
    showAllAppointmentsOnCalendar();
    showTodayOnCalendar();

    ui->actionSystem_Notifications->setChecked(true);
    this->notificationsFlag=true;
    checkForReminders();    
    checkForBirthdaysNextSevenDays();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::NewAppointment()
{
    DialogAppointment *appointmentDialog = new  DialogAppointment(this,&selectedDate);
    appointmentDialog->setModal(true);

    if (appointmentDialog->exec() == QDialog::Accepted ) {

        title =appointmentDialog->getTitle();
        location =appointmentDialog->getLocation();
        description= appointmentDialog->getDescription();
        selectedDate=appointmentDialog->getAppointmentDate(); //appointment date
        appointmentStartTime=appointmentDialog->getStartTime();
        appointmentEndTime=appointmentDialog->getEndTime();
        appointmentReminderRequested =appointmentDialog->getReminderRequested();
        appointmentReminderDate=appointmentDialog->getReminderDate();
        appointmentReminderTime=appointmentDialog->getReminderTime();

        Appointment a;
        a.m_title=title;
        a.m_location=location;
        a.m_description=description;
        a.m_appointmentDate=selectedDate.toString();
        a.m_appointmentStartTime=appointmentStartTime.toString();
        a.m_appointmentEndTime=appointmentEndTime.toString();
        a.m_reminderId=0;

        QString dateStr=QLatin1Char('(')+QString::number(selectedDate.day())+QLatin1Char('/')
                +QString::number(selectedDate.month())+QLatin1Char('/')
                +QString::number(selectedDate.year())+QLatin1Char(')');

        QString minutesStartStr = QStringLiteral("%1").arg(appointmentStartTime.minute(), 2, 10, QLatin1Char('0'));
        QString startTimeStr=QString::number(appointmentStartTime.hour(),'f',0)
                +QLatin1Char(':')+minutesStartStr;
        QString minutesEndStr = QStringLiteral("%1").arg(appointmentEndTime.minute(), 2, 10, QLatin1Char('0'));
        QString endTimeStr=QString::number(appointmentEndTime.hour(),'f',0)+
                QLatin1Char(':')+minutesEndStr;
        QString reminderDetails=title
                +QLatin1String(" (")+location
                + QLatin1String(") ")+dateStr+QLatin1String(" (")
                +startTimeStr+QLatin1String("->")+endTimeStr+QLatin1Char(')');


        Reminder r;
        r.m_details=reminderDetails;
        r.m_reminderDate=appointmentReminderDate.toString();
        r.m_reminderTime=appointmentReminderTime.toString();
        r.m_reminderRequest=appointmentReminderRequested;

        if (dbm.isOpen())
        {
            int reminderKey=dbm.addReminder(r);           
            reminderList.append(r);
            a.m_reminderId=reminderKey; //foreign link key
            bool success = dbm.addAppointment(a);
            if(success)
            {

                appointmentsList.append(a);
            }
        }
    }
    clearAllAppointmentsOnCalendar();
    LoadAppointmentsListFromDatabase();
    LoadReminderListFromDatabase();
    DisplayAppointmentsForDate(selectedDate); //updates the displayList using appointmentList
    showAllBirthdaysOnCalendar();
    showAllAppointmentsOnCalendar();
}

void MainWindow::UpdateAppointment(int dbID)
{
    Appointment currentAppointment = dbm.getAppointmentByID(dbID);

        DialogAppointment *appointmentDialog =
                new  DialogAppointment(this,&currentAppointment);
        appointmentDialog->setModal(true);

        if (appointmentDialog->exec() == QDialog::Accepted ) {
            //qDebug()<<"Delete request = "<<appointmentDialog->getDeleteRequested();

            if(appointmentDialog->getDeleteRequested())
            {
                int reminderKey = currentAppointment.m_reminderId;
                dbm.removeReminderById(reminderKey);
                dbm.removeAppointmentById(dbID);
                clearAllAppointmentsOnCalendar();
                LoadAppointmentsListFromDatabase();
                LoadReminderListFromDatabase();
                showAllAppointmentsOnCalendar();
                DisplayAppointmentsForDate(selectedDate);
                showAllBirthdaysOnCalendar();
                return;
            }

            this->title =appointmentDialog->getTitle();
            this->location=appointmentDialog->getLocation();
            this->description=appointmentDialog->getDescription();
            this->selectedDate=appointmentDialog->getAppointmentDate();
            this->appointmentStartTime=appointmentDialog->getStartTime();
            this->appointmentEndTime=appointmentDialog->getEndTime();

            this->appointmentReminderRequested=appointmentDialog->getReminderRequested();
            this->appointmentReminderDate=appointmentDialog->getReminderDate();
            this->appointmentReminderTime=appointmentDialog->getReminderTime();

            Appointment tmp;
            tmp.m_title=title;
            tmp.m_location=location;
            tmp.m_description=description;
            tmp.m_appointmentDate=selectedDate.toString();
            tmp.m_appointmentStartTime=this->appointmentStartTime.toString();
            tmp.m_appointmentEndTime=appointmentEndTime.toString();
            tmp.m_reminderId=0;

            QString dateStr=QLatin1Char('(')+QString::number(selectedDate.day())+QLatin1Char('/')
                    +QString::number(selectedDate.month())+QLatin1Char('/')
                    +QString::number(selectedDate.year())+QLatin1Char(')');

            QString minutesStartStr = QStringLiteral("%1").arg(appointmentStartTime.minute(), 2, 10, QLatin1Char('0'));
            QString startTimeStr=QString::number(appointmentStartTime.hour(),'f',0)
                    +QLatin1Char(':')+minutesStartStr;
            QString minutesEndStr = QStringLiteral("%1").arg(appointmentEndTime.minute(), 2, 10, QLatin1Char('0'));
            QString endTimeStr=QString::number(appointmentEndTime.hour(),'f',0)+
                    QLatin1Char(':')+minutesEndStr;
            QString reminderDetails=title
                    +QLatin1String(" (")+location
                    + QLatin1String(") ")+dateStr+QLatin1String(" (")
                    +startTimeStr+QLatin1String("->")+endTimeStr+QLatin1String(")");

            if (dbm.isOpen())
            {
                Reminder r = dbm.getReminderByID(currentAppointment.m_reminderId);
                r.m_details=reminderDetails;
                r.m_reminderDate=appointmentDialog->getReminderDate().toString();
                r.m_reminderTime=appointmentDialog->getReminderTime().toString();
                r.m_reminderRequest=appointmentDialog->getReminderRequested();
                dbm.updateReminder(r,currentAppointment.m_reminderId);


                if(dbm.updateAppointment(tmp,dbID))
                {
                    //qDebug()<<"appointment updated in database";
                }
            }

        }
        clearAllAppointmentsOnCalendar();
        LoadAppointmentsListFromDatabase();//clears and updates appointmentlist from db
        LoadReminderListFromDatabase();
        DisplayAppointmentsForDate(selectedDate);
        showAllBirthdaysOnCalendar();
        showAllAppointmentsOnCalendar();

}

void MainWindow::NewContact()
{
    //Add new contact
    DialogContact *contactDialog = new  DialogContact(this, &selectedDate);
    contactDialog->setModal(true);
    if (contactDialog->exec() == QDialog::Accepted ) {
        this->contactFirstName=contactDialog->getFirstName();
        this->contactMiddleNames=contactDialog->getMiddleNames();
        this->contactLastName=contactDialog->getLastName();
        this->contactEmail=contactDialog->getEmail();        
        this->street=contactDialog->getStreet();
        this->city=contactDialog->getCity();
        this->district=contactDialog->getDistrict();
        this->county=contactDialog->getCounty();
        this->postcode=contactDialog->getPostcode();
        this->country=contactDialog->getCountry();
        this->phoneNumber=contactDialog->getPhoneNumber();
        this->birthDate=contactDialog->getBirthDate();
        this->addToCalendar=contactDialog->getAddToCalendar();       
        this->birthDateId=contactDialog->getBirthDateId();

        Contact c;
        c.m_firstname=contactFirstName;
        c.m_midnames=contactMiddleNames;
        c.m_lastname=contactLastName;
        c.m_email=contactEmail;
        c.m_street=street;
        c.m_district=district;
        c.m_city=city;
        c.m_county=county;
        c.m_postcode=postcode;
        c.m_country=country;
        c.m_telephone=phoneNumber;
        c.m_birthdate=birthDate.toString();
        c.m_birthdayid=this->birthDateId;

        Birthday b;
        b.m_name=c.m_firstname+QLatin1Char(' ')+c.m_lastname;
        b.m_location=c.m_city;
        b.m_description=QLatin1String("Birthday");
        b.m_birthDate=birthDate.toString();
        b.m_addToCalendar=this->addToCalendar;

        if (dbm.isOpen())
        {
            int birthdayKey=dbm.addBirthday(b);           
            birthdayList.append(b);
            c.m_birthdayid=birthdayKey; //foreign link key
            bool success = dbm.addContact(c);

            if(success) //add contact to database
            {
                //qDebug()<<"contact added to database";
            }
        }
    }
    LoadContactsListFromDatabase(); //refresh contactslist here
    LoadBirthdayListFromDatabase();
    DisplayContactsOnTableView(); //display
    showAllBirthdaysOnCalendar();
    showAllAppointmentsOnCalendar();
}

void MainWindow::UpdateContact(int dbID)
{
    Contact currentContact = dbm.getContactByID(dbID);
    DialogContact *contactDialog = new  DialogContact(this,&currentContact); //(this,&selectedDate);

    contactDialog->setModal(true);
    if (contactDialog->exec() == QDialog::Accepted ) {

        if(contactDialog->getDeleteRequested())
        {
            int birthdaykey = currentContact.m_birthdayid;
            dbm.removeContactById(dbID);
            dbm.removeBirthdayById(birthdaykey);
            clearAllBirthdaysOnCalendar();
            LoadContactsListFromDatabase(); //clears and reloads contact lsit
            LoadBirthdayListFromDatabase();
            DisplayContactsOnTableView();
            DisplayBirthdaysForDate(this->selectedDate);
            showAllAppointmentsOnCalendar();
            return;  //gone!
        }        
        this->contactFirstName=contactDialog->getFirstName();
        this->contactLastName=contactDialog->getLastName();
        this->contactMiddleNames=contactDialog->getMiddleNames();
        this->contactEmail=contactDialog->getEmail();
        this->street=contactDialog->getStreet();
        this->district=contactDialog->getDistrict();
        this->city=contactDialog->getCity();
        this->county=contactDialog->getCounty();
        this->postcode=contactDialog->getPostcode();
        this->country=contactDialog->getCountry();
        this->phoneNumber=contactDialog->getPhoneNumber();
        this->birthDate=contactDialog->getBirthDate();
        this->addToCalendar=contactDialog->getAddToCalendar();       
        this->birthDateId=contactDialog->getBirthDateId();

        Contact c;
        c.m_firstname=contactFirstName;
        c.m_midnames=contactMiddleNames;
        c.m_lastname=contactLastName;
        c.m_email=contactEmail;
        c.m_street=street;
        c.m_district=district;
        c.m_city=city;
        c.m_county=county;
        c.m_postcode=postcode;
        c.m_country=country;
        c.m_telephone=phoneNumber;
        c.m_birthdate=birthDate.toString();


        if (dbm.isOpen())
        {
            Birthday b = dbm.getBirthdayByID(currentContact.m_birthdayid);
            b.m_name=contactFirstName+QLatin1Char(' ')+contactLastName;
            b.m_location=city;
            b.m_description=QLatin1String("Birthday");
            b.m_birthDate=birthDate.toString();
            b.m_addToCalendar=this->addToCalendar;

            dbm.updateBirthday(b,currentContact.m_birthdayid);

            if(dbm.updateContact(c, dbID)) //add contact to database
            {
                //qDebug()<<"contact added to database";               
            }

        }
    }
    clearAllBirthdaysOnCalendar();
    LoadContactsListFromDatabase(); //clears and reloads contactList
    LoadBirthdayListFromDatabase(); //clears and reloads birthdayList
    DisplayContactsOnTableView();
    DisplayBirthdaysForDate(this->selectedDate);    
    showAllBirthdaysOnCalendar();
    showAllAppointmentsOnCalendar();
}

void MainWindow::LoadAppointmentsListFromDatabase()
{
    //initialisation - do at startup
    appointmentsList.clear();
    QList<Appointment> tmpList = dbm.getAllAppointments();
    foreach(Appointment a, tmpList)
    {
        appointmentsList.append(a);
    }
}

void MainWindow::LoadContactsListFromDatabase()
{
    //initialisation - do at startup
    contactsList.clear();
    QList<Contact> tmpList = dbm.getAllContacts();
    foreach(Contact c, tmpList)
    {
        contactsList.append(c);
    }
}

void MainWindow::LoadBirthdayListFromDatabase()
{
    birthdayList.clear();    
    QList<Birthday> tmpList=dbm.getAllBirthdays();
    foreach(Birthday b, tmpList)
    {
        birthdayList.append(b); //borndate not current birthday
    }
}

void MainWindow::LoadReminderListFromDatabase()
{
    reminderList.clear();
    QList<Reminder> tmpList = dbm.getAllReminders();
    foreach(Reminder r, tmpList)
    {
        reminderList.append(r);
    }
}

void MainWindow::DisplayAppointmentsForDate(QDate theSelectedDate)
{
    dayModel->clearAllAppointments();//clear model
    foreach(Appointment a, appointmentsList)
    {
        QDate eventDate =QDate::fromString(a.m_appointmentDate);

        if (eventDate ==theSelectedDate)
        {
            dayModel->addAppointment(a);
        }
    }
    proxyModel->setSourceModel(dayModel);
    ui->tableView->setModel(proxyModel);
    ui->tableView->setSortingEnabled(true);
    ui->tableView->sortByColumn(2, Qt::AscendingOrder);//minutes
    ui->tableView->sortByColumn(1, Qt::AscendingOrder); //hour
    ui->tableView->horizontalHeader()->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView->setSortingEnabled(false);
    ui->tableView->hideColumn(0); //appointmentID
    ui->tableView->hideColumn(1); //sort hour
    ui->tableView->hideColumn(2); //sort min
    ui->tableView->hideColumn(3);
    ui->tableView->hideColumn(8);
}

void MainWindow::DisplayBirthdaysForDate(QDate theSelectedDate)
{
    birthdayModel->clearAllBirthdays();
    QDate currentDate =QDate::currentDate();
    foreach(Birthday b, birthdayList)
    {
        QDate borndate =QDate::fromString(b.m_birthDate);
        QDate currentBDay =QDate(currentDate.year(), borndate.month(), borndate.day());

        if(currentBDay==theSelectedDate){
            birthdayModel->addBirthday(b);
        }
    }
    ui->listViewBirthdays->setModel(birthdayModel);
}

void MainWindow::setCalendarOptions()
{
    ui->calendarWidget->setGridVisible(false);
    ui->calendarWidget->setVerticalHeaderFormat(ui->calendarWidget->NoVerticalHeader);

    todayFormat.setForeground(Qt::black);
    todayFormat.setFontItalic(true);
    todayFormat.setFontWeight(QFont::Bold);
    //todayFormat.setUnderlineStyle(QTextCharFormat::SingleUnderline);
    todayFormat.setFontUnderline(true);
    todayFormat.setBackground(Qt::white);

    eventDayFormat.setForeground(Qt::magenta);
    eventDayFormat.setBackground(Qt::lightGray);

    birthdayFormat.setForeground(Qt::black);
    birthdayFormat.setBackground(Qt::yellow);



    weekDayFormat.setForeground(Qt::black);
    weekDayFormat.setBackground(Qt::white);
    weekendFormat.setForeground(Qt::red);
    weekendFormat.setBackground(Qt::white);

}

void MainWindow::showAllAppointmentsOnCalendar()
{
    foreach(Appointment a, appointmentsList)
    {
        QDate appointmentDate =QDate::fromString(a.m_appointmentDate);
        ui->calendarWidget->setDateTextFormat(appointmentDate,eventDayFormat);
    }
    showTodayOnCalendar();
}

void MainWindow::clearAllAppointmentsOnCalendar()
{
    foreach(Appointment a, appointmentsList)
    {
        QDate appointmentDate =QDate::fromString(a.m_appointmentDate);

        if(appointmentDate.dayOfWeek()==Qt::Saturday || appointmentDate.dayOfWeek()==Qt::Sunday)
        {
            ui->calendarWidget->setDateTextFormat(appointmentDate,weekendFormat);
        }
        else {
            ui->calendarWidget->setDateTextFormat(appointmentDate,weekDayFormat);
        }
    }
    showTodayOnCalendar();
    ui->calendarWidget->repaint();
}

void MainWindow::showAllBirthdaysOnCalendar()
{
    QDate currentDate =QDate::currentDate();
    foreach(Birthday b, birthdayList)
    {
        if(b.m_addToCalendar==1){
            QDate borndate =QDate::fromString(b.m_birthDate);
            QDate currentBDay =QDate(currentDate.year(), borndate.month(), borndate.day());
            ui->calendarWidget->setDateTextFormat(currentBDay,birthdayFormat);
        }
    }
    showTodayOnCalendar();
}

void MainWindow::clearAllBirthdaysOnCalendar()
{
    QDate currentDate =QDate::currentDate();
    foreach(Birthday b, birthdayList)
    {
        QDate borndate =QDate::fromString(b.m_birthDate);
        QDate currentBDay =QDate(currentDate.year(), borndate.month(), borndate.day());
        if(currentBDay.dayOfWeek()==Qt::Saturday || currentBDay.dayOfWeek()==Qt::Sunday)
        {
            ui->calendarWidget->setDateTextFormat(currentBDay,weekendFormat);
        }
        else {
            ui->calendarWidget->setDateTextFormat(currentBDay,weekDayFormat);
        }
    }
    ui->calendarWidget->repaint();
    showTodayOnCalendar();
}

void MainWindow::showTodayOnCalendar()
{
    QDate currentDate =QDate::currentDate();
    ui->calendarWidget->setDateTextFormat(currentDate,todayFormat);
}

void MainWindow::clearTodayOnCalendar()
{
    QDate currentDate =QDate::currentDate();

    if(currentDate.dayOfWeek()==Qt::Saturday || currentDate.dayOfWeek()==Qt::Sunday)
    {
        ui->calendarWidget->setDateTextFormat(currentDate,weekendFormat);
    }
    else {
        ui->calendarWidget->setDateTextFormat(currentDate,weekDayFormat);
    }
}

void MainWindow::checkForReminders()
{
    reminderModel->clearAllReminders();
    QDate currentDate=QDate::currentDate();
    QTime currentTime= QTime::currentTime();
    int currentHour =currentTime.hour();
    int currentMinute = currentTime.minute();
    int currentSecond =currentTime.second();

    if(currentHour==12 && currentMinute==0 &&currentSecond==0 ) //midday
    {
        checkForBirthdaysNextSevenDays();
    }

    foreach(Reminder r, reminderList){

        QDate reminderDate =QDate::fromString(r.m_reminderDate);
        QTime reminderTime =QTime::fromString(r.m_reminderTime);
        int reminderHour =reminderTime.hour();
        int reminderMinute = reminderTime.minute();

        if(r.m_reminderRequest==1){

            if (reminderDate==currentDate)
            {
                reminderModel->addReminder(r);
                if(currentHour==reminderHour && currentMinute==reminderMinute &&currentSecond==0 ){
                    //fire single shot timer
                    if (notificationsFlag){
                        reminderSingleShot=r;
                        QTimer::singleShot(2000, this, SLOT(sendReminderMessage()));
                    }
                }
            }
        }
        ui->listViewReminders->setModel(reminderModel);
    } //if reminderrequest
}
void MainWindow::checkForBirthdaysNextSevenDays()
{
    birthdayReminderModel->clearAllBirthdays();
    QDate currentDate =QDate::currentDate();
    for(int i=1; i<8; i++)
    {
        foreach(Birthday b, birthdayList)
        {
            QDate bornDate = QDate::fromString(b.m_birthDate);
            QDate bdayDate=QDate(currentDate.year(),bornDate.month(),bornDate.day());
            if (bdayDate.addDays(-i)==currentDate) { //add all - day birthdays
                Birthday remBirthday =b;
                remBirthday.m_name=QLatin1String("(Resminder) ")+b.m_name;
                birthdayReminderModel->addBirthday(remBirthday);
            }
        }
    }
    ui->listViewBirthdays->setModel(birthdayReminderModel);
}

void MainWindow::exportAppointmentsXML()
{
    QDomDocument document;
    QDomElement root = document.createElement(QStringLiteral("Appointments"));
    document.appendChild(root);

    QList<Appointment> dbAppointmentList =dbm.getAllAppointments();
    foreach(Appointment a, dbAppointmentList){
        QString title=a.m_title;
        QString location=a.m_location;
        QString description =a.m_description;
        QString date =a.m_appointmentDate;
        QString startTime =a.m_appointmentStartTime;
        QString endTime=a.m_appointmentEndTime;

        QDomElement appointment = document.createElement(QStringLiteral("Appointment"));
        appointment.setAttribute(QStringLiteral("Title"),title);
        appointment.setAttribute(QStringLiteral("Location"), location);
        appointment.setAttribute(QStringLiteral("Description"),description);
        appointment.setAttribute(QStringLiteral("Date"),date);
        appointment.setAttribute(QStringLiteral("StartTime"),startTime);
        appointment.setAttribute(QStringLiteral("EndTime"),endTime);
        root.appendChild(appointment);
    }

    QString filename = QFileDialog::getSaveFileName(this, QStringLiteral("Save Appointments Xml"), QStringLiteral("."), QStringLiteral("Xml files (*.xml)"));
    QFile file(filename);
    if (!file.open(QFile::WriteOnly | QFile::Text)){

        //qDebug() << "Error saving XML file.";
        return;
    }
    else {
        QTextStream stream(&file);
        stream << document.toString();
        file.close();
        //qDebug() << "Finished";
    }
}


void MainWindow::exportContactsXML()
{
    QDomDocument document;
    QDomElement root = document.createElement(QStringLiteral("Contacts"));
    document.appendChild(root);

    QList<Contact> dbContactList =dbm.getAllContacts();
    foreach(Contact c, dbContactList){
        QString firstName=c.m_firstname;
        QString midName =c.m_midnames;
        QString lastName=c.m_lastname;
        QString email =c.m_email;
        QString street =c.m_street;
        QString district =c.m_district;
        QString city =c.m_city;
        QString county =c.m_county;
        QString postcode=c.m_postcode;
        QString country =c.m_country;
        QString telephone =c.m_telephone;
        QString birthdate =c.m_birthdate;

        QDomElement contact = document.createElement(QStringLiteral("Contact"));
        contact.setAttribute(QStringLiteral("FirstName"),firstName );
        contact.setAttribute(QStringLiteral("MidName"),midName);
        contact.setAttribute(QStringLiteral("LastName"), lastName);
        contact.setAttribute(QStringLiteral("Email"),email);
        contact.setAttribute(QStringLiteral("Street"),street);
        contact.setAttribute(QStringLiteral("District"),district);
        contact.setAttribute(QStringLiteral("City"),city);
        contact.setAttribute(QStringLiteral("County"),county);
        contact.setAttribute(QStringLiteral("Postcode"),postcode);
        contact.setAttribute(QStringLiteral("Country"),country);
        contact.setAttribute(QStringLiteral("Telephone"),telephone);
        contact.setAttribute(QStringLiteral("BirthDate"),birthdate);
        root.appendChild(contact);
    }

    QString filename = QFileDialog::getSaveFileName(this, QStringLiteral("Save Contacts Xml"), QStringLiteral("."), QStringLiteral("Xml files (*.xml)"));
    QFile file(filename);
    if (!file.open(QFile::WriteOnly | QFile::Text)){
        //qDebug() << "Error saving XML file.";
        return;
    }
    else {
        QTextStream stream(&file);
        stream << document.toString();
        file.close();
        //qDebug() << "Finished";
    }
}

void MainWindow::importContactsXML()
{
    QDomDocument document;
    QString filename = QFileDialog::getOpenFileName(this, QStringLiteral("Open Contacts Xml"), QStringLiteral("."), QStringLiteral("Xml files (*.xml)"));
    QFile file(filename);
    if (!file.open(QFile::ReadOnly | QFile::Text))
    {
        //qDebug() << "Failed to open XML file.";
        return;
    }
    else
    {
        if(!document.setContent(&file))
        {
            //qDebug() << "Failed to load document";
            return;
        }
        file.close();
    }


    QDomElement root = document.firstChildElement();
    QDomNodeList contact = root.elementsByTagName(QStringLiteral("Contact"));
    for(int i = 0; i < contact.count(); i++)
    {
        QDomNode contactnode = contact.at(i);
        if(contactnode.isElement())
        {
            QDomElement contact = contactnode.toElement();

            Contact c;
            c.m_firstname=contact.attribute(QStringLiteral("FirstName"));
            c.m_midnames=contact.attribute(QStringLiteral("MidName"));
            c.m_lastname=contact.attribute(QStringLiteral("LastName"));
            c.m_email=contact.attribute(QStringLiteral("Email"));
            c.m_street=contact.attribute(QStringLiteral("Street"));
            c.m_district=contact.attribute(QStringLiteral("District"));
            c.m_city=contact.attribute(QStringLiteral("City"));
            c.m_county=contact.attribute(QStringLiteral("County"));
            c.m_postcode=contact.attribute(QStringLiteral("Postcode"));
            c.m_country=contact.attribute(QStringLiteral("Country"));
            c.m_telephone=contact.attribute(QStringLiteral("Telephone"));
            c.m_birthdate=contact.attribute(QStringLiteral("BirthDate"));
            c.m_birthdayid=0;

            Birthday b;
            b.m_name=c.m_firstname+QLatin1Char(' ')+c.m_lastname;
            b.m_location=c.m_city;
            b.m_description=QLatin1String("Birthday");
            b.m_birthDate=contact.attribute(QStringLiteral("BirthDate"));
            b.m_addToCalendar=1;

            if (dbm.isOpen())
            {

                int birthdayKey=dbm.addBirthday(b);
                birthdayList.append(b);
                c.m_birthdayid=birthdayKey;
                bool success = dbm.addContact(c);
                if(success) //add contact to database
                {
                    //qDebug()<<"contact added to database";
                    contactsList.append(c);
                }
            }
        }
    }
    file.close();
    qDebug() << "Finished";

    LoadContactsListFromDatabase(); //refresh contactslist here
    LoadReminderListFromDatabase();
    DisplayContactsOnTableView(); //display
    showAllBirthdaysOnCalendar();
}


void MainWindow::importAppointmentsXML()
{

    QDomDocument document;
    QString filename = QFileDialog::getOpenFileName(this, QStringLiteral("Open Appointments Xml"), QStringLiteral("."), QStringLiteral("Xml files (*.xml)"));
    QFile file(filename);
    if (!file.open(QFile::ReadOnly | QFile::Text))
    {
        //qDebug() << "Failed to open XML file.";
        return;
    }
    else
    {
        if(!document.setContent(&file))
        {
           // qDebug() << "Failed to load document";
            return;
        }
        file.close();
    }

    QDomElement root = document.firstChildElement();
    QDomNodeList appointment = root.elementsByTagName(QStringLiteral("Appointment"));
    for(int i = 0; i < appointment.count(); i++)
    {
        QDomNode appointmentnode = appointment.at(i);
        if(appointmentnode.isElement())
        {
            QDomElement appointment = appointmentnode.toElement();

            Appointment a;
            a.m_title=appointment.attribute(QStringLiteral("Title"));
            a.m_location=appointment.attribute(QStringLiteral("Location"));
            a.m_description=appointment.attribute(QStringLiteral("Description"));
            a.m_appointmentDate=appointment.attribute(QStringLiteral("Date"));
            a.m_appointmentStartTime=appointment.attribute(QStringLiteral("StartTime"));
            a.m_appointmentEndTime=appointment.attribute(QStringLiteral("EndTime"));
            a.m_reminderId=0;

            QDate appDate =QDate::fromString(appointment.attribute(QStringLiteral("Date")));
            QTime appStartTime =QTime::fromString(appointment.attribute(QStringLiteral("StartTime")));
            QTime appEndTime =QTime::fromString(appointment.attribute(QStringLiteral("EndTime")));

            QString dateStr=QLatin1Char('(')+QString::number(appDate.day())+QLatin1Char('/')
                    +QString::number(appDate.month())+QLatin1Char('/')
                    +QString::number(appDate.year())+QLatin1Char(')');

            QString minutesStartStr = QStringLiteral("%1").arg(appStartTime.minute(), 2, 10, QLatin1Char('0'));
            QString startTimeStr=QString::number(appStartTime.hour(),'f',0)
                    +QLatin1Char(':')+minutesStartStr;
            QString minutesEndStr = QStringLiteral("%1").arg(appEndTime.minute(), 2, 10, QLatin1Char('0'));
            QString endTimeStr=QString::number(appEndTime.hour(),'f',0)+
                    QLatin1Char(':')+minutesEndStr;
            QString reminderDetails=appointment.attribute(QStringLiteral("Title"))
                    +QLatin1String(" (")+appointment.attribute(QStringLiteral("Location"))
                    + QLatin1String(") ")+dateStr+QLatin1String(" (")
                    +startTimeStr+QLatin1String("->")+endTimeStr+QLatin1Char(')');
            Reminder r;
            r.m_details=reminderDetails;
            r.m_reminderDate=appDate.toString();
            r.m_reminderTime=appStartTime.toString();
            r.m_reminderRequest=0;

            if (dbm.isOpen())
            {
                int reminderKey=dbm.addReminder(r);
                reminderList.append(r);
                a.m_reminderId=reminderKey;

                bool success = dbm.addAppointment(a);

                if(success)
                {
                    //qDebug()<<"appointment added to database";
                    //appointmentsList.append(a);

                }
            }
        }
    }

    file.close();
    //qDebug() << "Import Finished";
    LoadAppointmentsListFromDatabase();
    LoadReminderListFromDatabase();
    DisplayAppointmentsForDate(selectedDate); //updates the displayList using appointmentList
    showAllAppointmentsOnCalendar();
}

void MainWindow::timerUpdateSlot()
{
    QTime currentTime =QTime::currentTime();
    statusTimeLabel->setText(currentTime.toString(QStringLiteral("hh:mm:ss")));
    checkForReminders();
}

void MainWindow::sendReminderMessage()
{
    ui->statusBar->showMessage(QStringLiteral("New reminder notification .. "),10000); //10 seconds
    QString h=QStringLiteral("Reminder Notification");
    QString m=reminderSingleShot.m_details;

    MessageData *msg = new MessageData(h,m);
    DbusSessionMessage *dbus = new DbusSessionMessage(this);
    dbus->displayMessage(*msg);
}


void MainWindow::on_tableView_clicked(const QModelIndex &index)
{
    QVariant idd = index.sibling(index.row(),0).data();
    selectedAppointmentdDbId =idd.toInt();
}

void MainWindow::on_tableView_doubleClicked(const QModelIndex &index)
{
    QVariant idd = index.sibling(index.row(),0).data(); //id in column 0
    selectedAppointmentdDbId =idd.toInt();
    UpdateAppointment(selectedAppointmentdDbId);
}

void MainWindow::on_calendarWidget_clicked(const QDate &date)
{
    selectedDate=date;
    ui->labelSchedule->setText(QLatin1String("Appointments for ")+date.toString());
    DisplayAppointmentsForDate(selectedDate);
    DisplayBirthdaysForDate(selectedDate);
}

void MainWindow::on_actionNew_Appointment_triggered()
{
    NewAppointment();
}

void MainWindow::on_actionNew_Contact_triggered()
{
    NewContact();
}

void MainWindow::on_tableViewContacts_clicked(const QModelIndex &index)
{    
    QVariant idd = index.sibling(index.row(),0).data();
    selectedContactDbId =idd.toInt();    
    selectedContact=dbm.getContactByID(selectedContactDbId);
}

void MainWindow::on_tableViewContacts_doubleClicked(const QModelIndex &index)
{
    QVariant idd = index.sibling(index.row(),0).data();
    selectedContactDbId =idd.toInt();   
    UpdateContact(selectedContactDbId);
}

void MainWindow::on_pushButtonShowQuickDetails_clicked()
{
    ui->tableViewContacts->hideColumn(0); //index
    ui->tableViewContacts->hideColumn(13); //BithID

    if (quickViewFlag)
    {
        ui->tableViewContacts->hideColumn(2); //mid names
        ui->tableViewContacts->hideColumn(5); //street
        ui->tableViewContacts->hideColumn(6); //district
        ui->tableViewContacts->hideColumn(8); //county
        ui->tableViewContacts->hideColumn(9); //postcode
        ui->tableViewContacts->hideColumn(10); //country
        ui->tableViewContacts->hideColumn(11); //telephone
        quickViewFlag=false;
    }
    else {

        ui->tableViewContacts->showColumn(2); //mid names
        ui->tableViewContacts->showColumn(5); //street
        ui->tableViewContacts->showColumn(6); //district
        ui->tableViewContacts->showColumn(7); //city
        ui->tableViewContacts->showColumn(8); //county
        ui->tableViewContacts->showColumn(9); //postcode
        ui->tableViewContacts->showColumn(10); //country
        ui->tableViewContacts->showColumn(11); //telephone
        quickViewFlag=true;
    }
}

void MainWindow::on_pushButtonMailTo_clicked()
{   
    QString url=QLatin1String("mailto:?to=")+selectedContact.m_email+QLatin1String("&subject=Enter the subject&body=Enter message");
    QDesktopServices::openUrl(QUrl(url, QUrl::TolerantMode));
}

void MainWindow::on_actionDelete_All_Contacts_triggered()
{
    clearAllBirthdaysOnCalendar();
    dbm.removeAllContacts();
    dbm.removeAllBirthdays();
    contactsList.clear();
    birthdayList.clear();
    contactModel->clearAllContacts();
    birthdayModel->clearAllBirthdays();
    DisplayContactsOnTableView(); //should be nothing
    DisplayBirthdaysForDate(this->selectedDate);//should be nothing
}

void MainWindow::on_actionAbout_triggered()
{
    DialogAbout *aboutDialog = new DialogAbout(this);
    aboutDialog->setModal(false);
    aboutDialog->exec();
}

void MainWindow::on_actionClear_All_Reminder_Messages_triggered()
{
    reminderModel->clearAllReminders();
    ui->listViewReminders->setModel(reminderModel);
}

void MainWindow::on_actionDelete_All_Appointments_2_triggered()
{
    //qDebug()<<"Entering Delete All Appointments...";
    clearAllAppointmentsOnCalendar(); //uses appointment list
    dbm.removeAllAppointments();
    dbm.removeAllReminders();
    appointmentsList.clear();
    reminderList.clear();
    //Appointment models
    dayModel->clearAllAppointments();
    reminderModel->clearAllReminders();
    DisplayAppointmentsForDate(this->selectedDate); //should be no appointments
}

void MainWindow::on_actionExit_triggered()
{
    QApplication::quit();
}

void MainWindow::on_actionCalendar_Grid_triggered()
{
    if (ui->actionCalendar_Grid->isChecked())
    {
        ui->calendarWidget->setGridVisible(true);
    }
    else {
        ui->calendarWidget->setGridVisible(false);
    }
}

void MainWindow::on_actionCheckForReminders_triggered()
{
    checkForReminders();
}

void MainWindow::on_actionCheckForUpcomingBirthdays_triggered()
{
    checkForBirthdaysNextSevenDays();
}

void MainWindow::on_actionExport_Contacts_XML_triggered()
{
    exportContactsXML();
}

void MainWindow::on_actionExport_Appointments_XML_triggered()
{
    exportAppointmentsXML();
}

void MainWindow::on_ActionImport_Contacts_XML_triggered()
{
    importContactsXML();
}

void MainWindow::on_actionImport_Appointments_XML_triggered()
{
    importAppointmentsXML();
}

void MainWindow::on_actionCalendar_Weeks_triggered()
{
    if(ui->actionCalendar_Weeks->isChecked())
    {
        ui->calendarWidget->setVerticalHeaderFormat(ui->calendarWidget->ISOWeekNumbers);
    }
    else {
        ui->calendarWidget->setVerticalHeaderFormat(ui->calendarWidget->NoVerticalHeader);
    }
}

void MainWindow::DisplayContactsOnTableView()
{
    contactModel->clearAllContacts();

    foreach(Contact c, contactsList)
    {
        //qDebug()<<"Contact name = "<<c.m_firstname<<" "<<c.m_lastname;
        contactModel->AddContact(c);
    }
    proxyModelContacts->setSourceModel(contactModel);
    ui->tableViewContacts->setModel(proxyModelContacts);
    ui->tableViewContacts->setSortingEnabled(true);
    ui->tableViewContacts->sortByColumn(3, Qt::AscendingOrder); //sort on lastname
    ui->tableViewContacts->horizontalHeader()->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableViewContacts->setSortingEnabled(false);
    ui->tableViewContacts->hideColumn(0); //index
    ui->tableViewContacts->hideColumn(2); //mid names
    ui->tableViewContacts->hideColumn(5); //street
    ui->tableViewContacts->hideColumn(6); //district
    //city
    ui->tableViewContacts->hideColumn(8); //county
    ui->tableViewContacts->hideColumn(9); //postcode
    ui->tableViewContacts->hideColumn(10); //country
    ui->tableViewContacts->hideColumn(11); //telephone
    //birthday
    ui->tableViewContacts->hideColumn(13); //birthID
}

void MainWindow::on_pushButtonSortByFirstLastname_clicked()
{
    Qt::SortOrder order=Qt::AscendingOrder;   
    contactModel->clearAllContacts();
    foreach(Contact c, contactsList)
    {
        contactModel->AddContact(c);
    }

    if(sortOrderFlag==false){

        order=Qt::DescendingOrder;      
        sortOrderFlag=true;
    }
    else if (sortOrderFlag==true){

        order=Qt::AscendingOrder;
        sortOrderFlag=false;
    }

    proxyModelContacts->setSourceModel(contactModel);
    ui->tableViewContacts->setModel(proxyModelContacts);
    ui->tableViewContacts->setSortingEnabled(true);
    ui->tableViewContacts->sortByColumn(2,order);
    //ui->tableViewContacts->horizontalHeader()->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableViewContacts->hideColumn(0); //index
    ui->tableViewContacts->hideColumn(2); //mid names
    ui->tableViewContacts->hideColumn(5); //street
    ui->tableViewContacts->hideColumn(6); //district
    //city
    ui->tableViewContacts->hideColumn(8); //county
    ui->tableViewContacts->hideColumn(9); //postcode
    ui->tableViewContacts->hideColumn(10); //country
    ui->tableViewContacts->hideColumn(11); //telephone
    //birthday
    ui->tableViewContacts->hideColumn(13); //birthID
}
