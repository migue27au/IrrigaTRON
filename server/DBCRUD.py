#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import time
import pymysql

class DBCRUD:
	debug = False

	db_host = ""
	db_port = ""
	db_user = ""
	db_pass = ""
	db_name = ""

	def __init__(self, db_host, db_port, db_user, db_pass, db_name, debug = False):
		self.db_host = db_host
		self.db_port = db_port
		self.db_user = db_user
		self.db_pass = db_pass
		self.db_name = db_name
		self.debug = debug

	def __get_conector(self):
		try:
			return pymysql.connect(host=self.db_host, user=self.db_user, password=self.db_pass, db=self.db_name)
		except Exception as e:
			self.__printDebug("Exception: " + str(e))
			return None

	def drop_table(self, tablename):
		conn = self.__get_conector()
		cursorObj = conn.cursor()

		query = "DROP TABLE " + tablename + ";"
		
		if self.debug:
			self.__printDebug(query)

		cursorObj.execute(query)
		conn.commit()

	def create_table(self, tablename, attrib):
		conn = self.__get_conector()
		cursorObj = conn.cursor()

		query = "CREATE TABLE IF NOT EXISTS " + tablename + "(id INTEGER PRIMARY KEY AUTO_INCREMENT"
		for attrib_key in attrib.keys():
			query += ", " + attrib_key + " " + attrib[attrib_key]
		query+=");"

		if self.debug:
			self.__printDebug(query)

		cursorObj.execute(query)
		
		conn.commit()

	def list_tables(self):
		conn = self.__get_conector()
		cursorObj = conn.cursor()

		cursorObj.execute(query)

		return cursorObj.fetchall()

	#new_data dict
	def insert(self, tablename, data):
		conn = self.__get_conector()
		if len(data) > 0:
			cursorObj = conn.cursor()
			query = "INSERT INTO " + self.__sanitizor(tablename) + "("
			for key in data.keys():
				query+=self.__sanitizor(key)+","
			query = query[:-1]	#quitar Ãºltima coma
			query+=") VALUES("

			for key in data.keys():
				#self.__printDebug(type(data[key]))
				if type(data[key]) == str:
					query+="'"+self.__sanitizor(data[key])+"'"+","
				else:
					query+=self.__sanitizor(str(data[key]))+","
			
			query = query[:-1]	#quitar Ãºltima coma
			query+=");"

			if self.debug:
				self.__printDebug(query)

			cursorObj.execute(query)
			conn.commit()
		elif self.debug:
			self.__printDebug("Data must be one element at least")

	#new_data dict, condition dict
	def update(self, tablename, new_data, condition):
		conn = self.__get_conector()
		if len(new_data) > 0 and len(condition) > 0:
			cursorObj = conn.cursor()
			query="UPDATE " + self.__sanitizor(tablename) + " SET "
			
			for key in new_data.keys():
				if type(new_data[key]) == str:
					query+=key+"='"+self.__sanitizor(new_data[key])+"'"
				else:
					query+=key+"="+self.__sanitizor(str(new_data[key]))
				query += ","
			query = query[:-1]	#Quito la Ãºltima coma
			query+=" WHERE "
			for key in condition.keys():
				if type(condition[key]) == str:
					query+=key+"='"+self.__sanitizor(condition[key])+"'"
				else:
					query+=key+"="+self.__sanitizor(str(condition[key]))
				query+=" AND "

			query = query[:-5]	#Quito el Ãºltimo AND
			query+=";"
			if self.debug:
				self.__printDebug(query)

			cursorObj.execute(query)
			conn.commit()
		elif self.debug:
			self.__printDebug("New data and condition must be one element at least")

	def check(self, tablename, condition = None, attrib = None):
		conn = self.__get_conector()
		rows = self.select(tablename, condition, attrib)
		if len(rows) == 0 or self.select(tablename, condition, attrib)[0][0] == None:
			return False
		else:
			return True

	#Condition dict, attrib list
	def select(self, tablename, condition = None, attrib = None, distinct=False):
		conn = self.__get_conector()
		cursorObj = conn.cursor()
		query="SELECT "

		if distinct:
			query += "DISTINCT "

		if attrib:
			query+=self.__sanitizor(attrib)
		else:
			query+="*"

		query += " FROM " + self.__sanitizor(tablename)

		if condition:
			query += " WHERE "
			for key in condition.keys():
				if type(condition[key]) == str:
					query+=key+"='"+self.__sanitizor(condition[key])+"'"
				else:
					query+=key+"="+self.__sanitizor(str(condition[key]))
				query+=" AND "

			query = query[:-5]	#Quito el Ãºltimo AND
		query+=";"

		if self.debug:
			self.__printDebug(query)

		cursorObj.execute(query)

		return cursorObj.fetchall()

	#condition dict
	def delete(self, tablename, condition = None):
		conn = self.__get_conector()
		cursorObj = conn.cursor()
		query="DELETE FROM " + self.__sanitizor(tablename)
		if condition:
			query+=" WHERE "
		
		for key in condition.keys():
			if type(condition[key]) == str:
				query+=key+"='"+self.__sanitizor(condition[key])+"'"
			else:
				query+=key+"="+self.__sanitizor(str(condition[key]))
			query+=" AND "

		query = query[:-5]	#Quito el Ãºltimo AND
		query+=";"

		if self.debug:
			self.__printDebug(query)

		cursorObj.execute(query)
		
		conn.commit()

	def __sanitizor(self, input):
		return input.replace("'", "&apos;")

	def __desanitizor(self, input):
		return input.replace("&apos;", "'")


	def __printDebug(self, text):
		BOLD = '\033[1m'
		DEBUG = '\033[105m'
		ENDC = '\033[0m'

		current_time = time.strftime("%H:%M:%S", time.localtime())
		print(DEBUG + BOLD + "[D] DBCRUD " + ENDC + DEBUG + current_time + " -> " + ENDC + text)