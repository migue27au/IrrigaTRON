FROM python:3.8-slim-buster

WORKDIR /app
COPY requirements.txt ./
COPY main.py ./
COPY DAO.py ./
COPY DBCRUD.py ./
COPY templates/ ./templates/
COPY static/ ./static/

RUN pip install -r requirements.txt

EXPOSE 5000

CMD ["python", "main.py"]