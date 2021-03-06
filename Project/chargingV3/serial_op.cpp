#include "charging.h"
#include "CanProcess.h"
#pragma execution_character_set("utf-8")

static QString s_preStr;

QAction * charging::serial_scan(QAction * parentMenu)
{
	if (parentMenu == NULL)
		return NULL;
	QAction * action = new QAction("关闭设备", m_menuCom);
	action->setCheckable(true); action->setChecked(true);
	m_menuCom->addAction(action);

	QString temp = "", temp2, temp3, temp4; QAction * actionBase = NULL;
	char szCom[256] = { 0 }; //int iError = 0;
	m_iError = 0;
	SERIAL_PORT->getComInfo(szCom, m_iError);
	if (m_iError == 0){
	
		temp = QString("%1").arg(szCom);
		QStringList strlist = temp.split(",");
		for (int i = 0; i < strlist.size(); i++){
			if (strlist[i].isEmpty())
				strlist.removeAt(i);
		}
		// 串口排序
		for (int i = 0; i < strlist.size() - 1; i++){
			for (int j = 0; j < strlist.size() - 1 - i; j++){
				temp2 = strlist[j]; temp3 = strlist[j + 1];
				temp2.remove("COM"); temp3.remove("COM");
				if (temp2.toInt() > temp3.toInt()){
					temp4 = strlist[j];
					strlist[j] = strlist[j + 1];
					strlist[j + 1] = temp4;
				}
			}
		}
		//创建串口菜单
		for (int i = 0; i < strlist.size(); i++){
			if (strlist[i].isEmpty() == 0){
				QAction * action = new QAction(strlist[i], m_menuCom);  action->setCheckable(true);
				m_menuCom->addAction(action);
				actionBase = action;
			}
		}
		parentMenu->setMenu(m_menuCom);
	} 
	return actionBase;
	 
}

//菜单 串口点击事件
void charging::OnClickMenuCom(QAction * action)
{
	DEBUG_LOG(" 串口菜单点击事件开始执行\n");
	QList<QAction*> listAction = m_menuCom->actions();
	for (int i = 0; i < listAction.size(); i++){
		listAction[i]->setChecked(false);
	}

	if (action->text().indexOf("关闭串口") >= 0)
	{
		listAction[0]->setChecked(true);
		if (SERIAL_PORT->isOpen())
			SERIAL_PORT->ClosePort();
		isOpenSerialPort = false;
		printfDebugInfo("关闭串口" + s_preStr, enDebugInfoPriority::DebugInfoLevelOne); 
		s_preStr = ""; 
		return;
	}
	
	qDebug() << action->text();
	
	if (s_preStr != action->text() || !isOpenSerialPort)
	{
		s_preStr = action->text();
		if (SERIAL_PORT->isOpen())
			SERIAL_PORT->ClosePort(); 
		DEBUG_LOG(" 尝试打开串口" + action->text()+"\n");
		UINT portNo = action->text().remove("COM").toInt();		
		if (SERIAL_PORT->openPort(portNo))
		{
			isOpenSerialPort = true;	
			action->setChecked(true);
			//m_CommandQueue.init(&my_Serial , m_mapBattery.size()); 
			m_CommandQueue.init( m_mapBattery.size()); 
			printfDebugInfo("打开串口" + s_preStr + "成功", enDebugInfoPriority::DebugInfoLevelOne);
			DEBUG_LOG(" 打开串口" + s_preStr + "成功\n"); 
		}
		else{ 
			printfDebugInfo("打开串口" + s_preStr + "失败", enDebugInfoPriority::DebugInfoLevelOne);
			DEBUG_LOG(" 打开串口" + s_preStr + "失败\n");
			isOpenSerialPort = false;
		}
	} 
	if (!isOpenSerialPort)
	{
		for (int i = 0; i < listAction.size(); i++){
			if (i == 0)
				listAction[i]->setChecked(true);
			else
				listAction[i]->setChecked(false);
		}
	}

	//打开CAN设备进程  
	DEBUG_LOG(" 执行结束\n");
} 

QString temp_command;
//拼装命令，加入发送队列
void charging::toSend(QString strCommand, stCommand::enPriority enPriority)
{
	DEBUG_LOG(" 拼装命令" + strCommand + "，加入发送队列\n");
	stCommand stComm(packageCommand(strCommand), enPriority);
	emit AddCommamdIntoQueue(stComm);
}

//组装命令
QString  charging :: packageCommand(QString command)
{
	//如果最后没有逗号，就加一个
	if (command.at(command.size() - 1) != QChar(','))
		command = command + ",";
	//加上包头 *NF
	if (command.indexOf("*NF,") < 0)
		command = "*NF," + command;
	
	//计算xor校验和
	QByteArray myData(command.toLatin1());
	char xor;
	for (int i = 0; i < myData.size(); i++)
	{
		if (i == 0)
		{
			xor = myData.data()[0];
			continue;
		}
		xor = xor^myData[i];
	}
	if (xor == 0   )
		xor = 0x0b;

	command = command + xor;
	command = command + "," + (QString('\r')) + QString('\n');
	DEBUG_LOG(" 完成组装命令" + command + "\n");
	return command;
}

//测检收到的数据XOR
bool charging::detectRecvXOR(QString strContent)
{
	DEBUG_LOG(" 开始测检收到的数据XOR \n");
	bool ret = false;
	strContent.indexOf("");

	//计算xor校验和
	QByteArray myData(strContent.toLatin1());
	char xor = 0, xor1= 0;
	DEBUG_LOG(" 计算xor校验和" + strContent + " \n");
	int pos = myData.indexOf("\r\n");
	if (pos != -1)
	{
		xor = myData[pos - 2];
		QByteArray myData2 = myData.left(pos - 2);
		
		for (int i = 0; i < myData2.size(); i++)
		{
			if (i == 0)
			{
				xor1 = myData2.data()[0];
				continue;
			}
			xor1 = xor1^myData2[i];
		}
		if (xor1 == 0)
			xor1 = 0x0b;
		
		if (xor == xor1) //匹配成功
			ret = true;
		 
	}
	DEBUG_LOG(" xor校验匹配" + (ret?"成功":"失败")+" ,执行完毕\n");
	return  ret;
}

//解析收到的串口数据
void charging::readSerial(QString type, QString strContent, int iError)
{
	DEBUG_LOG(" 开始解析串口数据：" + strContent + " \n");
	int nPosEnd = strContent.lastIndexOf("\r\n");
	if (nPosEnd != -1)
		printfDebugInfo("I read: " + strContent.left(nPosEnd-2)+ "\\r\\n", enDebugInfoPriority::DebugInfoLevelThree);
	else
		printfDebugInfo("I read: " + strContent , enDebugInfoPriority::DebugInfoLevelThree);
	if (strContent.indexOf("*NF") == -1) //没有 *NF 不处理数据
		return;
	if (!detectRecvXOR(strContent))
	{
		printfDebugInfo("xor校验失败 接收内容"+ strContent  , enDebugInfoPriority::DebugInfoLevelThree, true); 
		return;
	}
	int loopA = -1, loopB = -1, nBatteryID = 0;
	QDateTime time = QDateTime::currentDateTime();//获取系统现在的时间 
	QString strTime, strChargerState; strTime.sprintf("\n");
	strTime += time.toString("hh:mm:ss");//设置显示格式 		 
	if (type == QChar('G') && iError == ChargingClosetError::noError)  //处理 充电状态 温度 电池在线情况
	{		 
		//COperatorFile::GetInstance()->writeLog(strTime + " G 读:" + strContent); 
		DEBUG_LOG(" 处理G协议：" + strContent + "\n");
		QString strContent2 = strContent;
		strContent2.remove("*NF,Y,");
		int len1 = strContent2.indexOf(",");
		if (len1 != -1)
		{ 
			QString strID = strContent2.left(len1), strBatteryID, strTemperature, strBatteryExist,strBatteryChanged;
			QString strChargerStateInfo = get_back_message_at(strContent2, 2);
			strTemperature = get_back_message_at(strContent2, 3);  //温度
			strBatteryExist = get_back_message_at(strContent2, 4); //在位
			strBatteryChanged = get_back_message_at(strContent2, 5); //在位时间是否变化
			int nChargerId = strID.toInt();
			DEBUG_LOG(" 温度:" + strTemperature + " 在位:" + strBatteryExist + " 在位时间是否变化:" + strBatteryChanged + "\n");
			MAP_CLOSET_IT itCloset = m_mapCloset.find(1);
			if (itCloset != m_mapCloset.end()){
				MAP_CHARGER_IT itCharger = itCloset->second.mapCharger.find(nChargerId);
				
				if (itCharger != itCloset->second.mapCharger.end())
				{ 
					nBatteryID = chargerIDtoBatteryId(nChargerId);
					int indexArray = batteryIDtoArrayIndex(QString::number(nBatteryID)); 
					//保存电池在线情况
					MAP_BATTERY_IT itBattery = itCloset->second.mapBattery.find(nBatteryID);
					MAP_BATTERY_MODEL_IT itBatteryMode = m_mapBatteryModel.find(itBattery->second.modelId);

					if (itBattery != itCloset->second.mapBattery.end()
						&& itBatteryMode != m_mapBatteryModel.end()
						&& itBatteryMode->second.balance == true )  //智能电池判断在位逻辑
					{
						battery_state[indexArray] = strBatteryExist == "1" ? "电池在线" : "未放置电池";
						itBattery->second.isExisted = strBatteryExist == "1" ? true : false;
						itBattery->second.isChanged = strBatteryChanged == "1" ? true : false;
						emit RefreshState(enRefreshType::BatteryState, indexArray);
					}
					
					//保存充电器在线状态 
					itCharger->second.bOnline = true;  
					//通知ui刷新充电器在线状态
					//emit RefreshState(enRefreshType::ChargerOnlineState, batteryIDtoArrayIndex(QString::number(itCharger->first)));

					DEBUG_LOG(" 更新充电器" + QString::number(itCharger->second.id) + "在线看门狗\n");
					//更新在线看门狗
					itCharger->second.nScanWatchDog = 0;
					//保存充电器 的 充电状态
					QString strChargerState = get_back_message_at(strContent2, 2);
					if (indexArray >= 0 && indexArray < charger_state.size()){
						//保存温度	判断是否过热
						float fTemp = itCharger->second.saveTemperature(strTemperature.toFloat());
						itCharger->second.isOverHeat =(fTemp >= m_fOverHeatTemperature) ? true : false;
						battery_temperature[indexArray].sprintf("%4.1f", fTemp);

						QString strChargerState = get_back_message_at(strContent2, 2);
						if (strChargerState == QChar('1'))//
						{							 
							if (itBattery->second.stRecord.pendingEndFlag)
							{
								itBattery->second.stRecord.pendingEndFlag = false;
								itBattery->second.stRecord.endChargeFlag = true; //充电器闲置，停止充电记录
							}
							if (itCharger->second.timeLockChargingState.elapsed() > 10000){
								charger_state[indexArray] = STATE_FREE;//"充电器闲置";
							}
						}
						else if (strChargerState == QChar('2'))
							charger_state[indexArray] = STATE_CHARGING;// "充电中";
						else if (strChargerState == QChar('3'))
							charger_state[indexArray] = STATE_DISCHARGING;//"放电中"; 
						DEBUG_LOG(" 更新电池" + nBatteryID + "状态：" + charger_state[indexArray] + "\n");
						emit RefreshState(enRefreshType::ChargerState, indexArray);
					}
					if (itCharger->second.timeLockChargingState.elapsed() > 2000){
						itCharger->second.isCharging = (strChargerState == QChar('2'));
						itCharger->second.isDisCharging = (strChargerState == QChar('3')); 
					}  
				
					
					//正在充电
					if (itCharger->second.isCharging)
					{
						//如果正在充电 计算充电时间,超时触发停止充电 add 20180527
						int iChargingTime = itCharger->second.calculateChargeTime();
						DEBUG_LOG(" 比较" + QString::number(itCharger->first) + "充电时间:" + QString::number(iChargingTime) + "与限制时间" + QString::number(m_nChargeLimitTime) + "\n");
						if (iChargingTime >= m_nChargeLimitTime)
						{
							int nBatteryID = chargerIDtoBatteryId(itCharger->first);
							if (nBatteryID != -1){
								QString strContent = QString::number(nBatteryID) + "电池关联" + \
									QString::number(itCharger->first) + "充电器,充电时间达到" + \
									QString::number(itCharger->second.calculateChargeTime()) + "分钟，触发停止";
								printfDebugInfo(strContent, enDebugInfoPriority::DebugInfoLevelOne);
								COperatorFile::GetInstance()->writeLog((QDateTime::currentDateTime()).toString("hh:mm:ss ") + strContent + "\n");
								DEBUG_LOG(strContent +"\n");
								//更新UI 
								int indexArray = batteryIDtoArrayIndex(QString::number(nBatteryID));
								if (charger_state[indexArray] != STATE_FREE/*"充电器闲置"*/ \
									&&itBattery->second.isApplyCharging == false){
									//判断是否有申请充电锁UI,如果没被申请，或者被申请了时间超过了10秒，则刷新UI
									stApplyInfo item = battery_apply_charging[indexArray];
									bool flag1 = (item.bApply == false || (item.bApply == true && item.timeLockUI.elapsed() > 10000));
									bool flag2 = itBattery->second.timeLockUI.elapsed() > 10000;
									int elapsed1 = itBattery->second.timeLockUI.elapsed();
									if ((item.bApply == false || (item.bApply == true && item.timeLockUI.elapsed() > 10000)) \
										&& itBattery->second.timeLockUI.elapsed() > 10000){
										charger_state[indexArray] = STATE_FREE;//"充电器闲置";
										emit RefreshState(enRefreshType::ChargerState, indexArray);
									}
								}
								//充电超时则发送停止命令
								stopByLocalID(QString::number(nBatteryID));
							}							
						}
						//如果过热,停止充电
						else if (itCharger->second.isOverHeat){
							DEBUG_LOG(QString::number(itCharger->first) +"过热,停止充电\n");
							toSend("P," + QString::number(itCharger->second.id), stCommand::hight);
						} 
					} 
					//充电器在以下情况读取电压
					//	1：智能电池在位并且充电时
					//	2：非智能电池
					bool readVol = false;
					if (itBatteryMode->second.balance == true
						&& itCharger->second.isCharging && itBattery->second.isExisted)
						readVol = true;
					if (itBatteryMode->second.balance == false)
						readVol = true;
					if (readVol){
						DEBUG_LOG(QString::number(itCharger->first) + "智能电池在位并且充电||非智能电池 触发读取电压命令\n");
						toSend("D," + QString::number(itCharger->second.id) + ",", stCommand::front);
					}
				}
			}
		}   
	}
	else if (type == QChar('D') && iError == ChargingClosetError::noError)//处理接收的电压、电流
	{
		//*NF, Y, 201, 6S, 0.00, 0.00, -, 
		//COperatorFile::GetInstance()->writeLog(strTime + " D 读:" + strContent); 
		QString strContent2 = strContent;		
		strContent2.remove("*NF,Y,");
		DEBUG_LOG(" 处理D协议：" + strContent + "\n");
		int len1 = strContent2.indexOf(",");		
		if (len1 != -1)
		{
			QString strID = strContent2.left(len1);
			int chargerId = strID.toInt();
			QString str;
			float vol = 0.0, curr = 0.0;  //电压电流
			MAP_CLOSET_IT itCloset = m_mapCloset.find(1);
			if (itCloset != m_mapCloset.end())
			{
				MAP_CHARGER_IT itCharger = itCloset->second.mapCharger.find(chargerId);
				if (itCharger != itCloset->second.mapCharger.end()){
					itCharger->second.bOnline = true; 
					DEBUG_LOG("通知ui刷新充电器" + strID + "在线状态\n");
					//通知ui刷新充电器在线状态
					emit RefreshState(enRefreshType::ChargerOnlineState,\
						batteryIDtoArrayIndex(QString::number(itCharger->first)));
				}
				int indexArray = batteryIDtoArrayIndex(QString::number(chargerIDtoBatteryId(chargerId)));
				if (indexArray >= 0 && indexArray < battery_state.size())
				{									
					MAP_BATTERY_IT itBattery = itCloset->second.mapBattery.find(chargerIDtoBatteryId(chargerId));
					if (itBattery != itCloset->second.mapBattery.end()){
						MAP_BATTERY_MODEL_IT itBatteryModel = m_mapBatteryModel.find(itBattery->second.modelId);
						str = itBatteryModel->second.connectType;
						int iConnectType = str.left(str.indexOf("S")).toInt();
						int nCommlength = strContent.length();
						if (nCommlength >= 27 && nCommlength < 77) 
						{  // 短命令大于等于27，小于77 
							//智能电池 总电压除与电池结构数
							str = get_back_message_at(strContent2, 3);
							vol = str.toFloat();
							if (vol > 0 && iConnectType > 0)
								vol = vol / iConnectType;
							//电流
							str = get_back_message_at(strContent2, 4);
							curr = str.toFloat();

						}
						else if (nCommlength > 77)
						{//1代 非智能电池的通讯协议 长命令大于77
							//非智能  读取每一节电池电压相加后除电池结构数	
							vol = 0; curr = 0;
							for (int i = 3; i <= (iConnectType*2+2); i++){
								if (i % 2 == 1){
									//电压
									str = get_back_message_at(strContent2, i );
									vol += str.toFloat();
								}
								else{
									//电流
									str = get_back_message_at(strContent2, i);
									curr += str.toFloat();
								} 
							}							
							if (vol > 0 && iConnectType > 0)
								vol = vol / iConnectType;
							if (curr > 0 && iConnectType > 0)
								curr = curr / iConnectType;
						}
						
						//非智能电池根据电压大于3.1V判断电池是否存在							
						if (itBatteryModel->second.balance == false)
						{
							battery_state[indexArray] = vol > 3.1 ? "电池在线" : "未放置电池";
							itBattery->second.isExisted = vol > 3.1 ? true : false;
						}
						//智能电池电池关闭时，并且电池未更换，读取原来的电压
						else  
						{
							float volTemp = battery_voltage[indexArray].toFloat(); 
							float currTemp = battery_current[indexArray].toFloat();
							if (itBattery->second.isExisted 
								&& itBattery->second.isChanged == false
								&& (volTemp > 3.25 && volTemp <= 4.5)
								&& (vol < 3.25 ))
							{
								vol = volTemp;
							}
						}
						//保存电压	电流	
						indexArray = batteryIDtoArrayIndex(itBattery->second.id);
						battery_voltage[indexArray].sprintf("%4.1f",vol);
						battery_current[indexArray].sprintf("%4.1f", curr);
						emit RefreshState(enRefreshType::BatteryVol, indexArray);
						itCharger->second.saveVoltage(vol);
						itCharger->second.fCurrent = curr;
						emit RefreshState(enRefreshType::BatteryState, indexArray);

						DEBUG_LOG("通知ui更新电池" + QString::fromLocal8Bit(itBattery->second.id) + " 电压:" + battery_voltage[indexArray] + " 电流:" + battery_current[indexArray]+ "\n");
					}
				}
			}
		}
	}
	else if (type == QChar('O') && iError == ChargingClosetError::noError)  //处理 充电命令
	{
		//COperatorFile::GetInstance()->writeLog(strTime + " O 读:" + strContent);
		DEBUG_LOG(" 处理O协议：" + strContent + "\n");
		QString strContent2 = strContent;
		strContent2.remove("*NF,Y,");
		int len1 = strContent2.indexOf(",");
		if (len1 != -1)
		{
			QString strID = strContent2.left(len1); //充电器ID

			int nBatteryId = chargerIDtoBatteryId(strID.toInt());
			if (nBatteryId != -1){
				int indexArray = batteryIDtoArrayIndex(strID);
				if (indexArray != -1)
				{
					QString strBatteryId = QString::number(nBatteryId);
					MAP_CLOSET_IT itCloset;	MAP_BATTERY_IT itBattery; MAP_BATTERY_MODEL_IT itBatteryModel; MAP_CHARGER_IT itCharger; MAP_LEVEL_IT itLevel;
					if (getBatteryIdRelatedInfo(strBatteryId, itCloset, itBattery, itBatteryModel, itCharger, itLevel))
					{
						QString strChargerState = get_back_message_at(strContent2, 2);
						if (strChargerState == "1") //充电成功 记录充电标志，提交服务器
						{
							itCharger->second.bOnline = true;
							if (itCharger->second.timeLockChargingState.elapsed() > 2000){								
								itCharger->second.isCharging = (strChargerState == QChar('1'));//充电成功
							}
							battery_charging_record[indexArray] = true;
							DEBUG_LOG(" 充电器：" + strID + "开始充电\n");
						}
					} 
				}
			}
		}		
	}
	else if (type == QChar('P') && iError == ChargingClosetError::noError)  //处理 停止命令
	{
		DEBUG_LOG(" 处理P协议：" + strContent + "\n");
		QString strContent2 = strContent;
		strContent2.remove("*NF,Y,");
		int len1 = strContent2.indexOf(",");
		if (len1 != -1)
		{
			QString strID = strContent2.left(len1); //充电器ID

			int nBatteryId = chargerIDtoBatteryId(strID.toInt());
			if (nBatteryId != -1){
				int indexArray = batteryIDtoArrayIndex(strID);
				if (indexArray != -1){					
					//停止充电后， 智能电池的充电电流强制设置为0a
					battery_current[indexArray] = QString("%1").arg(0);
					MAP_CLOSET_IT itCloset;	MAP_BATTERY_IT itBattery; MAP_BATTERY_MODEL_IT itBatteryModel; MAP_CHARGER_IT itCharger; MAP_LEVEL_IT itLevel;
					if (getBatteryIdRelatedInfo(QString::number(nBatteryId), itCloset, itBattery, itBatteryModel, itCharger,itLevel))
					{
						if(itBatteryModel->second.balance == true)
							itCharger->second.fCurrent = 0;
					}	
					DEBUG_LOG(" 充电器：" + strID + "闲置\n");
				}
			}
		}
	}
	else if (type == QChar('T') && iError == ChargingClosetError::noError)  //处理 继电器闭合回路命令
	{
	}
	else if (type == QChar('M') && iError == ChargingClosetError::noError)  //处理 查询电池存在、回路闭合状态命令
	{
	}
}
int charging::getCanDJIBattery(int CANID, int pos)
{	
	DEBUG_LOG(" 开始查找CanId&pos对应电池id\n");
	int temp1 = 0, temp2 = 1;
	MAP_CLOSET_IT itCloset = m_mapCloset.find(1);
	if (itCloset != m_mapCloset.end())
	{ 
		MAP_CHARGER_IT itCharger = itCloset->second.mapCharger.find(CANID);
		if (itCharger == itCloset->second.mapCharger.end())
		{
			printfDebugInfo(CANID + "未匹配充电器", enDebugInfoPriority::DebugInfoLevelOne, true);
			return 0;
		}
		MAP_LEVEL_IT itLevel = m_mapLevel.find(itCharger->second.nLevel);
		if (itLevel == m_mapLevel.end()){
			printfDebugInfo(CANID + "充电器未匹配层级", enDebugInfoPriority::DebugInfoLevelOne, true);
			return 0;
		} 

		for (auto itBattery : itLevel->second.mapBattery){
			if (itBattery.second.relatedCharger == CANID){
				if (temp2 == pos)
				{
					temp1 = itBattery.first;					
					break;
				}
				temp2++; 
			}
		}
	} 
	DEBUG_LOG(" 开始查找CanId：" + QString::number(CANID) + " 位置" + QString::number(pos) + "对应电池id" + QString::number(temp1) + "\n");
	return temp1;
}
// 解析接收到的CAN的内容param 1 内容。
void charging::onReadCAN(QString strContent)
{
	QStringList strList = strContent.split(",");
	if (strList.size() > 3)
	{
		DEBUG_LOG("接收到:" + strContent);
		//分析命令类型
		if (strList[1].compare("F1") == 0){
			//"S2C,F1,2,打开设备失败。" /"S2C,F1,0,打开设备成功。"

			if (strList[2].compare("0") == 0){
				GET_CAN->m_bOpenCanDevice = true;
				isOpenCANProcess = true;
				if (m_menuItemCan != nullptr){
					m_menuItemCan->blockSignals(true);
					m_menuItemCan->setChecked(true);
					m_menuItemCan->blockSignals(false);
					
				}				
				printfDebugInfo(strList[3], enDebugInfoPriority::DebugInfoLevelOne);
				DEBUG_LOG("解析F1协议:匹配S2C,F1,0,打开设备成功\n ");
			}
			else{
				printfDebugInfo(strList[3], enDebugInfoPriority::DebugInfoLevelOne, true);
				DEBUG_LOG("解析F1协议:匹配S2C,F1,2,打开设备失败\n ");
			}
			
		}
		else if (strList[1].compare("F4") == 0)
		{
			//认证通过			
			MAP_CLOSET_IT itCloset; MAP_CHARGER_IT itCharger;
			QVector<stCommand> vtStCommand;	itCloset = m_mapCloset.find(1);
			if (itCloset != m_mapCloset.end())
			{
				itCharger = itCloset->second.mapCharger.find(strList[2].toInt());
				if (itCharger != itCloset->second.mapCharger.end())
				{
					if (strList[3].toInt() == 0){
						DEBUG_LOG("解析F4协议:charger:" + strList[2] + " 认证通过\n ");
						itCharger->second.bOnline = true;
						MAP_LEVEL_IT itLevel = m_mapLevel.find(itCharger->second.nLevel);
						if (itLevel != m_mapLevel.end())
						{
							//电池在位情况
							for (auto itBattery : itLevel->second.mapBattery)
							{ 
								int indexArray = batteryIDtoArrayIndex(QString::fromLocal8Bit(itBattery.second.id));
								charger_state[indexArray] = STATE_FREE;//"充电闲置";
								emit RefreshState(enRefreshType::ChargerOnlineState, \
									batteryIDtoArrayIndex(QString::fromLocal8Bit(itBattery.second.id))); //更新在线状态 
							}
						}
					}					
				}
			}
		}
		else if (strList[1].compare("F8") == 0){
			DEBUG_LOG("解析F8协议:  \n ");
			//CAN ID
			int CANID = strList[2].toInt();	
			MAP_CLOSET_IT itCloset; itCloset = m_mapCloset.find(1);
			if (itCloset != m_mapCloset.end()  )  
			{ 
				MAP_CHARGER_IT itCharger = itCloset->second.mapCharger.find(CANID); MAP_LEVEL_IT itLevel;
				if (itCharger != itCloset->second.mapCharger.end())
				{
					itLevel = m_mapLevel.find(itCharger->second.nLevel);
					if (strList[3].toInt() == 0)//返回成功
					{
						itCharger->second.bOnline = true;
						itCharger->second.nScanWatchDog = 0; 
						
					}
					else if (strList[3].toInt() == 2) //超时
					{
						if (strList[4] == "收到数据：空")
							itCharger->second.bOnline = false;
						else{
							
						}
						//return;
					}
				}
				if (itLevel != m_mapLevel.end() && strList[3].toInt() == 0)//返回成功
				{
					itCharger = itLevel->second.mapCharger.find(CANID); itCharger->second.bOnline = true; itCharger->second.nScanWatchDog = 0;
					 
					//电池在位情况
					QString strOnline = strList[4];
					MAP_BATTERY_IT itBattery; int i;
					MAP_CLOSET_IT itCloset2;	MAP_BATTERY_IT itBattery2; MAP_BATTERY_MODEL_IT itBatteryModel2; MAP_CHARGER_IT itCharger2; MAP_LEVEL_IT itLevel2;				
					for (itBattery = itLevel->second.mapBattery.begin(), i = 0;
						itBattery != itLevel->second.mapBattery.end() && i < 15; itBattery++, i++)
					{
						itBattery->second.isExisted = (strOnline[i] == QChar('1'));
						if (getBatteryIdRelatedInfo(QString::fromLocal8Bit(itBattery->second.id), itCloset2, itBattery2, itBatteryModel2, itCharger2, itLevel2))
							itBattery2->second.isExisted = (strOnline[i] == QChar('1'));
						int indexArray = batteryIDtoArrayIndex(QString::fromLocal8Bit((itBattery->second.id)));
						battery_state[indexArray] = itBattery->second.isExisted ? "电池在线" : "未放置电池";
						if (itBattery->second.isExisted == false){
							battery_voltage[indexArray] = "0.0";
							battery_temperature[indexArray] = "0.0";
						} 
						indexArray = batteryIDtoArrayIndex(itBattery->second.id);
						emit RefreshState(enRefreshType::BatteryState, 	indexArray); //更新在线状态
						DEBUG_LOG(" 更新内存中电池:" + QString::fromLocal8Bit(itBattery->second.id) + " 在线状态:" + battery_state[indexArray] + "  \n ");
						if (itBattery->second.isExisted == false && itBattery->second.timeLockUI.elapsed() > 10000){
							charger_state[indexArray] = STATE_FREE;//"电池不在线，更新充电器闲置";
							if (itBattery2->second.stRecord.pendingEndFlag)
							{
								itBattery2->second.stRecord.pendingEndFlag = false;
								itBattery2->second.stRecord.endChargeFlag = true;  //电池不在线，停止充电
							}
							DEBUG_LOG(" 大于不在线时间10000，更新UI电池" + QString::fromLocal8Bit(itBattery->second.id) + " 在线状态:" + battery_state[indexArray] + "  \n ");
						}
					}

					QVector<int> vtnBatteryId; int pos_ = 0;
					//电池动态信息
					if (strList.size() >= 6)
					{						
						for (int i = 5; i < strList.size(); i++){
							QString strBattery = strList[i];
							QStringList strList2 = strBattery.split(" ");
							if (strList2.size() < 4)
								continue;
							//协议字段： [0]位置 [1]状态 [2]电压 [3]温度  //位置 状态 电压 温度
														
							int pos = strList2[0].toInt(); 
							int state = strList2[1].toInt(); //电池状态:	0x00 满电	0x01 充电中	0x02 放电中	0x03 静默 
							float vol = strList2[2].toFloat();//电压
							float tem = strList2[3].toFloat();//温度
							//赋值给内存中的电池映射
							int nBatteryId = getCanDJIBattery(CANID, pos);
							vtnBatteryId.append(nBatteryId);
							itBattery = itLevel->second.mapBattery.find(nBatteryId);
							if (itBattery != itLevel->second.mapBattery.end()){
								//赋值
								int indexArray = batteryIDtoArrayIndex(itBattery->second.id);
								battery_voltage[indexArray].sprintf("%4.1f", vol);
								battery_temperature[indexArray].sprintf("%4.1f", tem);
								//battery_current[indexArray].sprintf("%4.2f",curr);
								emit RefreshState(enRefreshType::BatteryVol, indexArray);
								//itCharger->second.saveVoltage(vol);
								//itCharger->second.fCurrent = curr;
								emit RefreshState(enRefreshType::BatteryState, indexArray);
								if (getBatteryIdRelatedInfo(QString::fromLocal8Bit(itBattery->second.id), itCloset2, itBattery2, itBatteryModel2, itCharger2, itLevel2))
								{
									itBattery->second.state = state;
									itBattery2->second.state = state;
								}
								if (state == 0 || state == 3)// 满电/静默
								{
									if (getBatteryIdRelatedInfo(QString::fromLocal8Bit(itBattery->second.id), itCloset2, itBattery2, itBatteryModel2, itCharger2, itLevel2))
									{
										if (itBattery2->second.stRecord.pendingEndFlag
											&& itBattery2->second.timeLockChargeRecord.elapsed() > 10000)  //
										{
											itBattery2->second.stRecord.pendingEndFlag = false;
											itBattery2->second.stRecord.endChargeFlag = true;//满电/静默，准备写入数据库记录停止充电,

											DEBUG_LOG(" 电池:" + QString::number(nBatteryId) + "大于不在线时间10000，满电/静默，准备写入数据库记录停止充电" + "\n");
										}
									}
									if (itBattery->second.timeLockUI.elapsed() > 10000){
										charger_state[indexArray] = STATE_FREE;//"充电器闲置";
										DEBUG_LOG(" 电池:" + QString::number(nBatteryId) + "大于不在线时间10000，" + charger_state[indexArray]+"\n");

									}
								}
								else if (state == 1)
									charger_state[indexArray] = STATE_CHARGING;// "充电中";
								else if (state == 2)
									charger_state[indexArray] = STATE_DISCHARGING;//"放电中"; 
								emit RefreshState(enRefreshType::ChargerState, indexArray);
								DEBUG_LOG(" 电池:" + QString::number(nBatteryId) +": "+ charger_state[indexArray]+"\n");
							}
						}
					} 
					////电池在位情况
					for (auto itBattery : itLevel->second.mapBattery)
					{
						//如果之前有动态信息，则跳过更新
						bool nextId = false;
						for (auto iD : vtnBatteryId)
						{
							if (itBattery.first == iD)
							{
								nextId = true;
								break;
							}
						}						
						if (nextId )
							continue;

						//
						if (itBattery.second.timeLockUI.elapsed() > 10000				){
							int indexArray = batteryIDtoArrayIndex(QString::fromLocal8Bit(itBattery.second.id));

							charger_state[indexArray] = STATE_FREE;//"充电闲置";
							battery_voltage[indexArray] = "0.0";
							battery_temperature[indexArray] = "0.0";
							emit RefreshState(enRefreshType::ChargerOnlineState, \
								batteryIDtoArrayIndex(QString::fromLocal8Bit(itBattery.second.id))); //更新在线状态
							if (getBatteryIdRelatedInfo(QString::fromLocal8Bit(itBattery.second.id), itCloset2, itBattery2, itBatteryModel2, itCharger2, itLevel2))
							{
								if (itBattery2->second.stRecord.pendingEndFlag)
								{
									itBattery2->second.stRecord.pendingEndFlag = false;
									itBattery2->second.stRecord.endChargeFlag = true;  //电池被拔出，停止充电
								}
							}
						}
					}
					//发送读取电池静态数据命令
					QString strCommad;	strCommad.sprintf("C2S,F12,%d,R", itCharger->second.id);
					stCommand stComm(strCommad, stCommand::enPriority::front); 
					stComm.chargerType = DJI_Charger;
					m_CommandQueue.addCommamd(stComm);

				}
			} 
		}
		else if (strList[1].compare("F9") == 0)
		{
			//充电反馈
			int CANID = strList[2].toInt();
		}
		else if (strList[1].compare("F10") == 0)
		{
			//放电反馈
			int CANID = strList[2].toInt();
		}

		else if (strList[1].compare("F12") == 0)
		{
			//电池静态数据
			int CANID = strList[2].toInt();
			MAP_CLOSET_IT itCloset; itCloset = m_mapCloset.find(1);
			if (itCloset != m_mapCloset.end())
			{
				MAP_CHARGER_IT itCharger = itCloset->second.mapCharger.find(CANID); MAP_LEVEL_IT itLevel;
				if (itCharger != itCloset->second.mapCharger.end())
				{
					itLevel = m_mapLevel.find(itCharger->second.nLevel);
					if (strList[3].toInt() == 0)//返回成功
					{
						itCharger->second.bOnline = true;
						itCharger->second.nScanWatchDog = 0;

					}
					else if (strList[3].toInt() == 2) //超时
					{
						if (strList[4] == "收到数据：空")
							itCharger->second.bOnline = false;
						else{

						}
					}
				}
				if (itLevel != m_mapLevel.end() && strList[3].toInt() == 0)//返回成功
				{ 
					//解析静态电池动态信息
					if (strList.size() >= 5)
					{
						QString strBattery;
						for (int i = 4; i < strList.size(); i++){
							strBattery = strList[i];
							QStringList strList2 = strBattery.split(" ");
							if (strList2.size() < 7)
								continue;
							//[0]位置,[1]寿命百分比, [2]循环次数,[3]SN,[4]生产时间,[5]容量,[6]loader版本, [7]app版本  
							int pos = strList2[0].toInt();
							int nBatteryId = getCanDJIBattery(CANID, pos);
							strBattery = "电池编号:" + QString::number(nBatteryId);
							strBattery += ", 寿命百分比:" + strList2[1];
							strBattery += "%%, 循环次数:" + strList2[2];
							strBattery += ", SN:" + strList2[3];
							strBattery += ", 生产时间:" + strList2[4];
							strBattery += ", 容量:"+strList2[5];
							strBattery += "mAh";//", loader版本:" + strList2[6];
							//strBattery += ", app版本 :"+strList2[7];
							//添加到 数据显示对话框
							if (m_dataDlg)
								m_dataDlg->addData(nBatteryId, strBattery);
							 
						}
					}

				}
			}
		}

	}
}
