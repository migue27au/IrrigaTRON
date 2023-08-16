import DBCRUD,json

#---SQLETE MODULE---
class PlantDTO:
	_id = 0
	name = ""
	description = ""
	pump = 0
	tank_height = 0
	inner_keys = ""

	def fillByRow(self, row):
		self._id = row[0]
		self.name = row[1]
		self.description = row[2]
		self.pump = row[3]
		self.tank_height = row[4]
		self.inner_keys = row[5]

	def __init__(self, name="", description = "", pump=0, tank_height=0, inner_keys=""):
		self.name = name
		self.description = description
		self.pump = pump
		self.tank_height = tank_height
		self.inner_keys = inner_keys

	def __repr__(self):
		return str(json.dumps(self.__dict__, sort_keys=True))

class DataDTO:
	_id = 0
	timestamp = 0
	key = ""
	value = 0

	def fillByRow(self, row):
		self._id = row[0]
		self.timestamp = int(row[1])
		self.key = row[2]
		self.value = float(row[3])

	def __init__(self, timestamp="", key="", value=""):
		self.timestamp = timestamp
		self.key = key
		self.value = value

	def __repr__(self):
		return str(json.dumps(self.__dict__, sort_keys=True))

class WateringDTO:
	_id = 0
	plant_id = 0
	plant = None
	timestamp = 0

	def fillByRow(self, row):
		self._id = row[0]
		self.plant_id = row[1]
		self.timestamp = int(row[2])

	def __init__(self, plant=None, timestamp=0):
		self.plant = plant
		self.timestamp = timestamp

	def __repr__(self):
		return str(json.dumps(self.__dict__, sort_keys=True))

class ConditionDTO:
	_id = 0
	plant_id = 0
	plant = None
	group = 0
	key = ""
	condition = 0
	value = 0

	def fillByRow(self, row):
		self._id = row[0]
		self.plant_id = int(row[1])
		self.group = int(row[2])
		self.key = row[3]
		self.condition = row[4]
		self.value = row[5]

	def __init__(self, plant="", group="", key="", condition="", value=""):
		self.plant = plant
		self.group = group
		self.key = key
		self.condition = condition
		self.value = value

	def __repr__(self):
		return str(json.dumps(self.__dict__, sort_keys=True))

class DAO:
	db = None

	def __init__(self, db_host, db_port, db_user, db_pass, db_name, debug = False):
		self.db = DBCRUD.DBCRUD(db_host, db_port, db_user, db_pass, db_name, debug)

	def init(self):
		#Tabla que almacena los proyectos
		self.db.create_table('plants', {'name':'VARCHAR(256)','description':'VARCHAR(1024)', 'pump':'INT', 'tank_height':'FLOAT', 'inner_keys':'VARCHAR(1024)'})
		self.db.create_table('datas', {'timestamp':'INT', 'inner_key':'VARCHAR(1024)', 'inner_value':'FLOAT'})
		self.db.create_table('waterings', {'plant_id':'INT', 'timestamp':'INT'})
		self.db.create_table('conditions', {'plant_id':'INT', 'inner_group':'INT', 'inner_key':'VARCHAR(1024)', 'inner_condition':'VARCHAR(32)', 'inner_value':'FLOAT'})


	#### PLANTS ####
	def updatePlant(self, plant):
		if not self.db.check("plants", {'name':plant.name}):
			self.db.insert("plants", {'name':plant.name, 'description':plant.description, 'pump':plant.pump, 'tank_height':plant.tank_height, 'inner_keys':plant.inner_keys})
		else:
			self.db.update("plants", {'description':plant.description, 'pump':plant.pump, 'tank_height':plant.tank_height, 'inner_keys':plant.inner_keys}, {'name':plant.name})

	def getPlants(self):
		rows = self.db.select("plants")
		plants = []
		for row in rows:
			plant = PlantDTO()
			plant.fillByRow(row)
			plants.append(plant)
		return plants
	def getPlantByName(self, name):
		rows = self.db.select("plants", {'name': name})
		if len(rows) == 0:
			return None
		else:
			plant = PlantDTO()
			plant.fillByRow(rows[0])

			return plant

	def getPlantByPump(self, pump):
		rows = self.db.select("plants", {'pump': pump})
		if len(rows) == 0:
			return None
		else:
			plant = PlantDTO()
			plant.fillByRow(rows[0])

			return plant


	#### DATAS ####
	def updateData(self, data):
		if not self.db.check("datas", {'timestamp':data.timestamp, 'inner_key':data.key}):
			self.db.insert("datas", {'timestamp':data.timestamp, 'inner_key':data.key, 'inner_value':data.value})
		else:
			self.db.update("datas", {'inner_value':data.value}, {'timestamp':data.timestamp, 'inner_key':data.key})

	def getDatasByPlant(self, plant, key=None):
		if key == None:
			inners_keys = plant.inner_keys.split(",")
		else:
			inners_keys = key.split(",")
			
		datas = []
		for inner_key in plant.inner_keys.split(","):
			rows = self.db.select("datas", {"inner_key":inner_key})
			for row in rows:
				data = DataDTO()
				data.fillByRow(row)

				if inner_key == "tank_height":
					data.value = int(((plant.tank_height-data.value)/plant.tank_height)*100)
					print(data.value)
				datas.append(data)

		return datas

	def getLastDatasByPlant(self, plant, key=None):
		if key == None:
			inners_keys = plant.inner_keys.split(",")
		else:
			inners_keys = key.split(",")

		datas = []
		for inner_key in inners_keys:
			rows = self.db.select("datas", {"inner_key":inner_key})
			if len(rows) > 0:
				data = DataDTO()
				data.fillByRow(rows[-1])

				if inner_key == "tank_height":
					data.value = int(((plant.tank_height-data.value)/plant.tank_height)*100)

				datas.append(data)

		return datas

	def getInnerKeys(self):
		inner_keys = []
		for row in self.db.select("datas", None, "inner_key", distinct=True):
			inner_keys.append(row[0])
		return inner_keys

	#### WATERINGS ####
	def createWatering(self, watering):
		if not self.db.check("waterings", {'plant_id':watering.plant._id, 'timestamp':watering.timestamp}):
			self.db.insert("waterings", {'plant_id':watering.plant._id, 'timestamp':watering.timestamp})

	def getWateringsByPlant(self, plant):
		rows = self.db.select("waterings", {'plant_id':plant._id})
		waterings = []
		for row in rows:
			watering = WateringDTO()
			watering.fillByRow(row)
			waterings.append(watering)
		return waterings

	#### CONDITIONS ####
	def updateCondition(self, condition):
		if not self.db.check("conditions", {'plant_id':condition.plant._id, 'inner_group':condition.group, 'inner_key':condition.key}):
			self.db.insert("conditions", {'plant_id':condition.plant._id, 'inner_group':condition.group, 'inner_key':condition.key, 'inner_condition':condition.condition, 'inner_value':condition.value})
		else:
			self.db.update("conditions", {'inner_condition':condition.condition, 'inner_value':condition.value}, {'plant_id':condition.plant._id, 'inner_group':condition.group, 'inner_key':condition.key})

	def deleteCondition(self, condition):
		if self.db.check("conditions", {'plant_id':condition.plant._id, 'inner_group':condition.group, 'inner_key':condition.key}):
			self.db.delete("conditions", {'plant_id':condition.plant._id, 'inner_group':condition.group, 'inner_key':condition.key})

	def getConditionsByPlant(self, plant, group=None):
		if group == None:
			rows = self.db.select("conditions", {'plant_id':plant._id})
		else:
			rows = self.db.select("conditions", {'plant_id':plant._id, 'inner_group':group})

		conditions = []
		for row in rows:
			condition = ConditionDTO()
			condition.fillByRow(row)
			condition.plant = plant
			conditions.append(condition)
		return conditions