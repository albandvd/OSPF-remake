.PHONY: up demo logs down clean

## Build and start the full lab (routers, terminals, and the demo scenario)
up:
	docker compose up --build

## Re-run just the scenario against an already-running lab
demo:
	docker compose run --rm --no-deps scenario

## Follow logs from every container
logs:
	docker compose logs -f

## Stop and remove containers and networks (keeps images)
down:
	docker compose down

## Full teardown: containers, networks, volumes, and locally built images
clean:
	docker compose down -v --rmi local
