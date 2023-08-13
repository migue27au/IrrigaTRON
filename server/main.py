#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse, sys, time, os, json, datetime, string, random, traceback
from flask import Flask, render_template, request, session, redirect, abort, send_file, make_response
from flask_cors import CORS

import DAO

class Config(object):
	SECRET_KEY = ''
	alphabet = string.ascii_letters + string.digits + string.punctuation
	for i in range(32):
		SECRET_KEY += ''.join(random.choice(alphabet))
	SESSION_PERMANENT = False

app = Flask(__name__)
CORS(app)
app.config.from_object(Config())

dao = DAO.DAO("127.0.0.1", 3306, "IRRIGATRON", "IRRIGATRON", "IRRIGATRON", debug=True)
dao.init()

must_water = {}

### HOME ###
@app.route('/test', methods=['GET'])
def test():
	response = {'message': 'OK'}
	return make_response(json.dumps(response), 200)

### HOME ###
@app.route('/', methods=['GET'])
@app.route('/home', methods=['GET'])
def home():
	plants = dao.getPlants()
	return render_template('home.html', plants=plants)

@app.route('/new_plant', methods=['POST'])
def newPlant():
	data = request.form
	print(data)
	plant = DAO.PlantDTO(data["name"], data["description"], data["pump"])
	
	if(len(dao.getPlants()) >= 8):
		print("cannot create a new plant")
	else:
		dao.updatePlant(plant)

	return redirect('/home')

@app.route('/plant/<plant_name>', methods=['GET'])
def homePlant(plant_name):
	plant = dao.getPlantByName(plant_name)
	plants = dao.getPlants()
	
	inner_keys = dao.getInnerKeys()
	print(inner_keys)

	plant_data = dao.getDatasByPlant(plant)

	groups = {}
	for group in dao.getConditionsByPlant(plant):
		if str(group.group) not in groups.keys():
			groups[str(group.group)] = []

		groups[str(group.group)].append(group)

	print(groups.keys())

	condition_groups = []

	for condition in groups.keys():
		condition_groups.append({"id":condition, "conditions":groups[condition]})

	inner_keys.append("time")	##añado el time como inner key

	return render_template('plant.html', plant=plant, plants=plants, inner_keys=inner_keys, plant_data=plant_data, condition_groups=condition_groups)

@app.route('/plant/<plant_name>/force_water', methods=['POST'])
def forceWater(plant_name):
	must_water[plant_name] = True
	print(must_water[plant_name])

	return redirect('/plant/{}'.format(plant_name))

@app.route('/plant/<plant_name>/update_keys', methods=['POST'])
def updateKeys(plant_name):
	plant = dao.getPlantByName(plant_name)
	plants = dao.getPlants()

	data = request.form
	print(data)

	inner_keys = []
	for e in data.keys():
		inner_keys.append(e)
	plant.inner_keys = ','.join(inner_keys)
	dao.updatePlant(plant)

	return redirect('/plant/{}'.format(plant_name))


@app.route('/plant/<plant_name>/new_condition', methods=['POST'])
def newCondition(plant_name):
	plant = dao.getPlantByName(plant_name)
	plants = dao.getPlants()
	
	data = request.form
	print(data)

	condition = DAO.ConditionDTO(plant, data["group_id"], data["key"], data["condition"], data["value"])

	dao.updateCondition(condition)

	return redirect('/plant/{}'.format(plant_name))

@app.route('/plant/<plant_name>/delete_condition', methods=['POST'])
def deleteCondition(plant_name):
	plant = dao.getPlantByName(plant_name)
	plants = dao.getPlants()
	
	data = request.form
	print(data)

	condition = DAO.ConditionDTO(plant, data["group_id"], data["key"])

	dao.deleteCondition(condition)

	return redirect('/plant/{}'.format(plant_name))


## ARDUINO ###
@app.route('/upload', methods=['POST'])
def upload():
	data = request.get_json()

	print(data);

	if "datas" in data.keys():
		for d in data["datas"]:
			data_dto = DAO.DataDTO(timestamp=int(time.time()), key=list(d.keys())[0], value=d[list(d.keys())[0]])
			dao.updateData(data_dto)

		response = {'message': 'Data OK'}
		return make_response(json.dumps(response), 201)
	else:
		response = {'message': 'datas is mandatory'}
		return make_response(json.dumps(response), 400)

@app.route('/watering', methods=['POST'])
def watering():
	data = request.get_json()

	print(data);

	if "pump" in data.keys():
		plant = dao.getPlantByPump(data["pump"])
		if plant != None:
			watering = DAO.WateringDTO(plant, timestamp=int(time.time()))
			dao.createWatering(watering)

		response = {'message': 'Data OK'}
		return make_response(json.dumps(response), 201)
	else:
		response = {'message': 'datas is mandatory'}
		return make_response(json.dumps(response), 400)

@app.route('/water', methods=['POST'])
def water():
	data = request.get_json()

	print(data);

	pump_states = {"pump0":False, "pump1":False, "pump2":False, "pump3":False, "pump4":False, "pump5":False, "pump6":False, "pump7":False}

	for index,key in enumerate(pump_states.keys()):
		plant = dao.getPlantByPump(index)
		if plant != None:
			if plant.name in must_water.keys() and must_water[plant.name] == True:
				print("force watering")
				must_water[plant.name] = False
				pump_states[key] = True
				
			else:
				conditions = dao.getConditionsByPlant(plant)

				conditions.sort(key=lambda x: x.group)

				groups = {}
				for condition in conditions:
					if str(condition.group) not in groups.keys():
						groups[str(condition.group)] = []
					groups[str(condition.group)].append(condition)

				for key in groups.keys():
					condition_true_counter = 0

					for condition in groups[key]:
						if condition.key != "time":
							last_data = dao.getLastDatasByPlant(plant, condition.key)
							if last_data != None:
								if ((condition.condition == "greater" and last_data > condition.value)
									or (condition.condition == "lower" and last_data < condition.value)):
									condition_true_counter += 1
						else:
							now = datetime.now().time()
							time_in_condition = str(condition.value).split(".")
							if len(str(condition.value).split(".")) == 1:
								time_in_condition.append(0)
							time_in_condition[0] = int(time_in_condition[0])
							time_in_condition[1] = int(time_in_condition[1])

							if ((condition.condition == "greater" and now.hour >= time_in_condition[0] and now.minute > time_in_condition[1])
								or (condition.condition == "lower" and now.hour <= time_in_condition[0] and now.minute < time_in_condition[1])):
								condition_true_counter += 1

					if condition_true_counter >= len(groups[key]):
						pump_states[key] = True
	
	print(json.dumps(pump_states))
	return make_response(json.dumps(pump_states), 200)
	

#app.run(host=host, debug=True, port=sqlete_defaults.http_port, ssl_context=(sqlete_defaults.sqlete_framework_path+'/sqlete/certs/cert.pem', sqlete_defaults.sqlete_framework_path+'/sqlete/certs/key.pem'))
app.run(host="0.0.0.0", debug=True, port=80)